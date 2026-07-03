# Testing

## Current Validation
- `idf.py build`
- `make -C components/wifi_manager/host_test test` — host-side tests for
  the Wi-Fi connection policy state machine and profile blob codec (pure
  C, no ESP-IDF, built with plain gcc + ASan/UBSan). Runs in CI as the
  `host-tests` job in `.github/workflows/build.yml`.

## Planned Tests
- host-side parser tests
- hardware tests are manual for now (see `docs/validation/`)