# Hardware validation: market_data_client (Phase 7)

## Environment

- Date: 2026-07-03
- Board: JC4880P443C_I_W
- Target: esp32p4
- ESP-IDF: v6.0.1
- Port: /dev/cu.usbmodem101
- Wi-Fi: existing saved profile in NVS (no dev-cred injection, no `erase-flash`
  used - see `feedback_erase_flash_not_erase_region` on why targeted erases
  are avoided for encrypted-NVS state)

## Method

Phase 7's scope is the `market_data_client` library only - it is not wired
into `app_main` (that's Phase 8). To validate it end-to-end against the real
Binance API, a temporary smoke-test task was added to `app_main` (waits for
`time_sync_is_synced()`, then calls
`market_data_client_fetch_symbol_status("BTCUSDT", ...)` and
`market_data_client_fetch_klines_24h_5m("BTCUSDT", ...)`, logging results),
flashed, and monitored. The temporary task and the `market_data_client`
`REQUIRES` line in `main/CMakeLists.txt` were removed afterward and the board
was reflashed with the clean (repo-matching) firmware.

```sh
idf.py -p /dev/cu.usbmodem101 flash
```

`idf.py monitor` requires an interactive TTY, unavailable in the automated
session this test ran in - the serial port was read directly with `pyserial`
at 115200 baud instead (same effect, plain log capture).

## Observed logs

```text
I (1279) market_terminal: ESP32 Market Data Terminal started
I (1519) wifi_manager: ESP-Hosted SDIO mempool budget: required=32592 internal_dma_largest=180224 spiram_dma_largest=0
I (4519) wifi_manager: Wi-Fi station started over ESP-Hosted link
I (4539) wifi_manager: ESP32-C6 co-processor firmware: 2.12.8
I (4549) wifi_manager: Wi-Fi profile storage available: yes
I (5569) market_terminal: market_data smoke test: waiting for time sync...
I (5649) wifi_manager: Connecting to 'TV' (origin=1)
I (10849) wifi_manager: Connecting to 'TV' (origin=1)
I (12079) esp_netif_handlers: sta ip: 192.168.100.238, mask: 255.255.255.0, gw: 192.168.100.1
I (12549) time_sync: System time synced via SNTP
I (13159) market_terminal: market_data smoke test: fetch_symbol_status('BTCUSDT') -> err=0 is_trading=1 has_spot=1
I (15239) market_terminal: market_data smoke test: fetch_klines_24h_5m('BTCUSDT') -> count=288
I (15239) market_terminal: market_data smoke test: first candle open_time_ms=1783025700000 close=61584.78
I (15239) market_terminal: market_data smoke test: last candle open_time_ms=1783111800000 close=62819.99
```

## Result

**Passed.**

- `market_data_client_fetch_symbol_status("BTCUSDT", ...)` returned
  `MARKET_DATA_OK` with `is_trading=true, has_spot_permission=true` - a real
  HTTPS round trip to `api.binance.com/api/v3/exchangeInfo`, TLS validated
  via the certificate bundle, JSON parsed by the custom streaming
  `market_data_symbol_parser`.
- `market_data_client_fetch_klines_24h_5m("BTCUSDT", ...)` returned
  `MARKET_DATA_OK` with exactly `count=288` - the full v1 request (24h of
  5-minute candles) round-tripped and streamed through
  `market_data_klines_parser` with no truncation and no capacity error.
- First/last candle `open_time_ms` are 86,100,000 ms (86,100 s) apart, i.e.
  287 gaps x 5 minutes = 1,435 minutes = 23h55m - exactly the expected span
  for 288 candles at 5-minute granularity. Close prices (~$61.6k-$62.8k) are
  plausible real BTC/USDT values for the request time.
- No crashes, panics, or watchdog resets. Total time from boot to both
  requests completing: ~14 seconds (dominated by the ~6.5s Wi-Fi connect
  retry, not the HTTP requests themselves - the two Binance calls together
  took ~2 seconds).
- This exercises time_sync's Phase 6 gating exactly as designed: the smoke
  test blocked on `time_sync_is_synced()` before either call, and both calls
  succeeded on the first attempt with no `MARKET_DATA_ERR_NOT_SYNCED` or TLS
  certificate errors from an unsynced clock.

## Not exercised on hardware in this session

- `MARKET_DATA_ERR_SYMBOL_NOT_FOUND` / `MARKET_DATA_ERR_RATE_LIMITED` /
  `MARKET_DATA_ERR_HTTP_STATUS` paths (would require an invalid symbol or
  deliberately tripping Binance's rate limit) - covered by host-side tests
  at the parser level, not re-verified against real HTTP error responses.
- `SETTINGS_API_REGION_US` (`api.binance.us`) base URL selection - the board
  used the default `SETTINGS_API_REGION_INTL` (`api.binance.com`); switching
  the persisted setting and re-running was not done in this session.
- Timeout handling (`MARKET_DATA_ERR_TIMEOUT`) under a genuinely slow/stalled
  connection - the test network's round trip was fast enough that the
  10-second `MARKET_DATA_HTTP_TIMEOUT_MS` was never approached.
