# Testing

## Current Validation
- `idf.py build`
- `make -C components/wifi_manager/host_test test` — host-side tests for
  the Wi-Fi connection policy state machine and profile blob codec (pure
  C, no ESP-IDF, built with plain gcc + ASan/UBSan). Runs in CI as the
  `host-tests` job in `.github/workflows/build.yml`.
- `make -C components/settings_store/host_test test` — host-side tests for
  the display/symbols/locale/api_region settings blob codec (seal/validate,
  corruption and out-of-range rejection; pure C, same gcc + ASan/UBSan
  setup). Runs in the same `host-tests` CI job.
- `make -C components/market_data_client/host_test test` — host-side tests
  for the Binance REST client's pure logic: the query-string builder, the
  incremental JSON tokenizer (including token reassembly across arbitrary
  chunk boundaries), and the two streaming grammars (exchangeInfo symbol
  status, klines rows) covering valid/malformed/edge-case JSON. Pure C, same
  gcc + ASan/UBSan setup, no ESP-IDF dependency. Runs in the same
  `host-tests` CI job.
- `make -C components/market_data_ws_client/host_test test` — host-side
  tests for the WebSocket kline stream client's pure logic: the
  combined-stream URL builder, and the streaming grammar for one
  `@kline_1s` event (valid/malformed/non-kline/unknown-key/type-mismatch
  cases, plus the same chunk-boundary reassembly check as the REST parsers).
  Pure C, same gcc + ASan/UBSan setup, no ESP-IDF dependency. Runs in the
  same `host-tests` CI job.
- `make -C components/app_state/host_test test` — host-side tests for the
  retry/backoff/resync policy and the `@kline_1s` → 5m-candle merge
  aggregator (merge-in-place, append-with-eviction, stale/out-of-order
  ignore, volume/trade-count accumulation only on a closed 1s kline). Pure
  C, same gcc + ASan/UBSan setup, no ESP-IDF dependency. Runs in the same
  `host-tests` CI job.

## Planned Tests
- host-side parser tests
- hardware tests are manual for now (see `docs/validation/`)