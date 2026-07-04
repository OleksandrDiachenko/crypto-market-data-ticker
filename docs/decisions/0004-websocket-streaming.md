# 0004: WebSocket kline streaming — 1s stream, aggregation, and queue architecture

## Status

Accepted (2026-07-05)

## Context

Phase 8 gave the watchlist's market data a REST bootstrap/resync model
(`components/app_state`, one 288-candle 5m-interval PSRAM array per symbol,
replaced wholesale on every successful sync). Between syncs - every ~5
minutes, or longer under retry/backoff - that data is static. Phase 9 closes
this gap: a WebSocket stream keeps the most recent candle live between REST
syncs, which REST resync then continues to correct/backfill as before.

Three sub-decisions came up while designing `components/market_data_ws_client`
and `app_state`'s new writer path.

## Decision

### 1. Reopening 0002's WebSocket rejection - now justified

[0002](0002-market-data-client.md) rejected WebSockets for Phase 7/8: "a
persistent connection... adds a second long-lived network dependency" for
no benefit at 5-minute polling granularity. That reasoning held while the
only consumer of kline data was a periodic REST resync. It no longer holds
once live, sub-5-minute price movement is itself a product requirement (the
terminal should show the current candle ticking, not just its last synced
snapshot) - at which point a second long-lived connection is buying
something concrete, not just architectural symmetry with an idealized
"streaming is better" default.

### 2. Subscribe to `@kline_1s`, not `@kline_5m` directly, with an aggregation rule

The stream subscribes to `{symbol}@kline_1s` for every watchlist symbol - not
the 5-minute interval REST already polls. Each 1s update is applied with
standard exchange candle-update rules (`app_state_kline_merge_apply()`,
`components/app_state/src/app_state_kline_merge.c`):

- Same 5-minute bucket as the last stored candle (`floor(open_time_ms /
  300000) * 300000` matches): merge in place - `high`/`low` widen, `close`
  tracks the latest update on every call (lowest-latency price), but
  `volume`/`number_of_trades` accumulate **only when the 1s kline has
  closed** (`k.x == true`). Binance re-sends the same in-progress 1s kline
  repeatedly before it closes; accumulating on every non-final update would
  double-count.
- Next bucket: append a new candle, evicting the oldest to stay within the
  fixed 288-entry capacity.
- Older bucket (stale/out-of-order, e.g. after a reconnect gap): ignored.
  REST's existing gap-detection resync (`app_state_sync_task.c`) remains the
  source of truth for real gaps - this phase does not add out-of-order
  buffering on top of it.

This is a deliberate accuracy/complexity trade-off with one known edge case:
if the update queue (below) drops the *first* update of a new bucket under
load, that bucket's `open` seeds from a later price until the next REST
resync self-heals it. Accepted, not a bug.

### 3. WebSocket → queue → app_state, mirroring `wifi_manager`'s existing precedent

`components/market_data_ws_client` owns the connection and a FreeRTOS queue
of decoded `market_data_kline_update_t`, exposed via
`market_data_ws_client_get_update_queue()` - this is the same owner-owns-
queue, single-consumer pattern `wifi_manager` already uses for its own event
queue (`wifi_manager_get_event_queue()`, consumed solely by
`app_state_sync_task.c` per that file's own "sole consumer of this queue by
design" comment). The WS client's event-handler callback (running in
`esp_websocket_client`'s own task) parses each frame into a small fixed-size
struct and does a non-blocking `xQueueSend(..., 0)` with `ESP_LOGW` +
drop-on-full - the exact mirror of `wifi_manager.c`'s `publish_event()`.

A **separate** task (`app_state_ws_task`, not `app_state_sync_task`)
consumes the queue with a **blocking** `xQueueReceive(..., portMAX_DELAY)`.
`app_state_sync_task` already polls its own queue non-blockingly every 2s
because it has REST HTTP calls to make in between; folding 1s-latency queue
consumption into that same loop would stall live updates behind a REST
round-trip. The new task has nothing else to do, so blocking receive is both
simpler and lower-latency.

`app_state_apply_kline_update()` takes the same `s_lock` mutex as
`app_state_record_success()`/`_error()` and touches only `klines`/
`kline_count` - it never touches `state`/`retry_attempt`/`last_error`, which
stay exclusively owned by the REST sync path. Lock hold time for both
writers is a `memcpy`/small merge, not an I/O wait, so contention between
the two writers is not a practical concern.

### 4. Combined-stream URL, not per-symbol connections or a runtime SUBSCRIBE frame

One connection - `wss://.../stream?streams=btcusdt@kline_1s/ethusdt@kline_1s/...`
- for the whole watchlist, region-selected the same way as REST's base URL
(`settings_store_load_api_region()`). Subscription is the URL itself; no
runtime `{"method":"SUBSCRIBE",...}` frame is sent. (The roadmap's Phase 9
text mentions "SUBSCRIBE" generically; this is a deliberate simplification a
reviewer should not read as a missing feature.)

### 5. Parsing reuses the existing pure JSON scanner - now promoted to public

`market_data_ws_stream_parser.c` (a key-driven-object-with-skip grammar, like
`market_data_symbol_parser.c`, since envelope/kline keys can arrive in any
order) is built on `market_data_json_scanner.c` - the same incremental
tokenizer `market_data_klines_parser.c` and `market_data_symbol_parser.c`
already use, for the same reason 0002 gives (bounded memory, no cJSON). Since
this scanner is now shared across two components,
`market_data_json_scanner.h` moved from `market_data_client`'s private
`src/` to its public `include/` - `market_data_client`'s other grammar
headers (`klines_parser.h`, `symbol_parser.h`, `url.h`) stay private; only
the scanner itself became a public, reusable dependency.

### 6. `esp_websocket_client`'s own reconnect is used as-is

`disable_auto_reconnect` is left at its default (`false`). Reconnection is a
**fixed delay** (`reconnect_timeout_ms`, defaults to 10s), not exponential
backoff - unlike `app_state_retry_policy`'s hand-rolled exponential backoff
for REST. This is accepted rather than hand-rolling a second backoff
implementation: a fixed 10s retry still prevents a reconnect busy-loop, and
the WebSocket link is a soft dependency (Wi-Fi/time_sync-style) whose loss
only costs live-update latency, not correctness.

## Consequences

- `market_data_ws_client` REQUIRES `market_data_client` (for
  `market_data_err_t` and the now-public JSON scanner) and `settings_store`
  (region); `app_state` REQUIRES `market_data_ws_client` (header-only, for
  `market_data_kline_update_t`) alongside its existing `market_data_client`
  dependency. No circular dependency: `market_data_ws_client` does not
  depend on `app_state`.
- New managed component dependency: `espressif/esp_websocket_client`
  (`components/market_data_ws_client/idf_component.yml`).
- `components/market_data_ws_client/host_test/` and
  `components/app_state/host_test/` (previously present but not wired into
  CI) now both build with plain gcc + ASan/UBSan and run in the
  `host-tests` CI job, same as every other component's host tests.
- No persistence of live-updated candles to NVS/flash - PSRAM only, rebuilt
  via REST bootstrap + WS deltas on every reboot, same as Phase 8.
- No runtime subscription rebuild when the watchlist changes - no
  watchlist-edit UI exists yet (per 0003's own Consequences section). When
  one ships, it will need to call `market_data_ws_client_stop()` then
  `_start()` again with the new symbol list.
- Thread-safety is not a practical risk under this design: only
  `app_state_ws_task_start()` (called once from `main.c`) ever calls into
  `market_data_ws_client`'s API. This would become a real risk only if a
  future watchlist-edit UI calls `_stop()`/`_start()` again from a different
  task than the one that originally started it.

## Alternatives considered

- **Subscribing directly to `@kline_5m`:** simpler (no aggregation logic
  needed), but Binance only pushes a kline-stream update on trade activity
  within that interval, at a cadence no faster than trades occur - `@kline_1s`
  gives materially lower latency for the "current candle is ticking" goal at
  the cost of the merge/append logic above.
- **Raw (non-combined) stream per symbol, N connections:** avoids the
  combined-stream envelope's extra `"stream"`/`"data"` nesting, but costs a
  TLS connection per watchlist symbol (up to 8) instead of one.
- **Folding WS queue consumption into `app_state_sync_task`:** rejected -
  that task's REST HTTP calls would add seconds of latency to live update
  processing.
