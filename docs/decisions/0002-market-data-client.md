# 0002: Market data client — region, TLS, and streaming JSON parsing

## Status

Accepted (2026-07-03)

## Context

Phase 7 needs a REST client for Binance's public market data API
(`GET /api/v3/exchangeInfo`, `GET /api/v3/klines`) — no WebSocket (klines
are a 5-minute-granularity pull model, not worth a second long-lived
connection), no API keys, no trading logic. Three sub-decisions came up
while designing `components/market_data_client`.

## Decision

### 1. API region is a runtime `settings_store` domain, not a Kconfig flag

Binance splits its public API by region: `https://api.binance.com` for most
users, `https://api.binance.us` for US-regulated users. This is modeled as
a new `api_region_settings_t` domain (`settings_codec.h`/`settings_store.h`),
following the exact seal/validate/CRC pattern already used for
display/symbols/locale, rather than a `menuconfig` compile-time choice.
Region is switchable without reflashing, consistent with every other piece
of user-facing configuration in this project.

### 2. TLS via the existing curated certificate bundle, not cert pinning

`sdkconfig` already has `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL=y`
(the curated Mozilla CA bundle) enabled from the base ESP-IDF template.
`market_data_http.c` uses `esp_crt_bundle_attach` in
`esp_http_client_config_t` rather than pinning Binance's certificate.
Zero extra maintenance, no re-pinning work when Binance rotates certs —
a real cost for a portfolio project with no ops team watching for expiry.

### 3. Custom streaming JSON parser instead of a DOM library (cJSON)

The klines response for the v1 use case (288 five-minute candles) is
roughly 47KB of JSON. A DOM parser (cJSON, or ESP-IDF's bundled `json`
component) would need to either buffer the whole response body before
parsing, or build a parse tree of ~3,700+ nodes (~64 bytes each ≈ 230KB) —
both wasteful on a device where internal SRAM is already shared with
LVGL's display buffers.

Instead, `market_data_json_scanner.c` is a minimal incremental (SAX-style)
tokenizer: it is fed raw HTTP response bytes as they arrive from
`esp_http_client_read()`, one ~1KB chunk at a time, and produces JSON
tokens one at a time with no parse tree. `market_data_klines_parser.c` and
`market_data_symbol_parser.c` layer a small purpose-built grammar state
machine on top (rigid 12-field-row grammar for klines; a key-driven walk
with generic structural skip for exchangeInfo's less predictable object
shape), writing decoded fields directly into the caller-owned output
struct as each row/field completes.

This bounds memory to roughly 1KB (HTTP chunk buffer) + ~100 bytes (parser
state) instead of a ~47KB body buffer plus a ~230KB DOM tree — at the cost
of writing and testing two small purpose-built grammars instead of using a
general-purpose library. Host tests explicitly verify that feeding the same
document as one chunk, one byte at a time, or in randomly-sized chunks
produces byte-identical output (`test_market_data_klines_parser.c`), which
is the correctness property a streaming parser must hold and a DOM parser
gets for free.

## Consequences

- No third-party JSON library is vendored or linked; `market_data_client`
  has zero new external dependencies beyond ESP-IDF's own `esp_http_client`
  / `esp-tls`.
- `components/market_data_client/host_test/` builds with plain gcc +
  ASan/UBSan, same as every other component's host tests — no `$IDF_PATH`
  or package-manager dependency needed in CI.
- The two grammars are tied to Binance's exact response shapes (12-field
  kline rows; `symbols[0].status`/`permissionSets`). A schema change on
  Binance's side would require updating the grammar, not just a library
  version bump — an accepted trade-off given the API is a stable, versioned
  public endpoint (`/api/v3/`).
- `market_data_client_fetch_symbol_status()` / `_fetch_klines()` both check
  `time_sync_is_synced()` before opening a connection, returning
  `MARKET_DATA_ERR_NOT_SYNCED` immediately rather than letting an unsynced
  clock surface as a generic, hard-to-diagnose TLS certificate error.

## Alternatives considered

- **cJSON (ESP-IDF's bundled `json` component) for firmware, apt-installed
  `libcjson-dev` for host tests:** avoids writing custom grammars, but
  introduces a version-skew risk between whatever Ubuntu's package manager
  provides and ESP-IDF's bundled version, and adds an `apt-get` step none
  of this project's other host_test targets need.
- **Vendoring cJSON's single-file amalgamation into the component:** keeps
  firmware and host tests on identical code, but is still a DOM parser
  (doesn't solve the memory problem) and would be the first vendored
  third-party source in this repo.
- **WebSocket streams instead of REST polling:** rejected early — a
  persistent connection (reconnect/keepalive logic) buys nothing at
  5-minute candle granularity and adds a second long-lived network
  dependency alongside Wi-Fi.
