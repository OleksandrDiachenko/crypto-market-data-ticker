# Hardware validation: OTA firmware update via GitHub Releases (Phase 10)

## Environment

- Date: 2026-07-05
- Board: JC4880P443C_I_W
- Target: esp32p4, ESP-IDF v6.0.2
- Port: /dev/cu.usbmodem101
- Wi-Fi: existing saved profile ("TV") in encrypted NVS
- Watchlist: BTCUSDT, ETHUSDT

## Method

`idf.py monitor` requires an interactive TTY, unavailable in this session - the serial
port was read (and, for this phase, written to) directly with `pyserial` at 115200
baud instead of a real terminal.

## Step 1: baseline flash of the new partition table (no `erase-flash`)

```sh
idf.py -p /dev/cu.usbmodem101 flash
```

`partitions.csv`'s `factory` -> `otadata`+`ota_0`+`ota_1` change (0006) was flashed
directly onto a board previously running the old table, with no `erase-flash`. Boot
confirmed everything the ADR predicted would survive actually did:

```text
I (1122) app_init: App version:      0.10.0
...
I (4626) wifi_manager: ESP32-C6 co-processor firmware: 2.12.9
I (4606) wifi_manager_slave_ota: Co-processor firmware already up to date
I (4627) wifi_profile_store: Wi-Fi profile store ready (encrypted)
I (4627) wifi_manager: Wi-Fi profile storage available: yes
I (4637) app_state: Loaded 2 watchlist symbol(s)
```

Encrypted Wi-Fi profile store, co-processor `slave_fw` image, and the watchlist all
intact - `nvs_keys`/`wifi_cfg`/`slave_fw` offsets were untouched by design.

## Bug found during validation: OTA background check didn't retry for 6h (fixed)

First boot's background check ran before Wi-Fi/time_sync were ready:

```text
W (4637) app_state_ota: Release check skipped/failed: 2
```

(`2` = `OTA_CLIENT_ERR_NOT_SYNCED`.) `app_state_ota_task.c`'s `run_check()`
unconditionally stamped `s_last_check_ms` regardless of outcome, so the next attempt
wouldn't happen for a full `OTA_CHECK_INTERVAL_MS` (6h) instead of retrying soon like
every other soft-dependency task in this project (`app_state_sync_task` retries every
loop iteration until Wi-Fi/time_sync are up). Fixed to only advance the timestamp on
success and re-arm an immediate retry on failure - confirmed on the next boot:

```text
W (4636) app_state_ota: Release check skipped/failed: 2
W (66196) app_state_ota: Release check skipped/failed: 5
```

(Second attempt ~60s later, now getting as far as an actual HTTP response - `5` =
`OTA_CLIENT_ERR_HTTP_STATUS`, expected since no GitHub Release existed yet at that
point.)

## Finding: the manual CLI trigger's physical console isn't reachable over this port

`main/ota_console.c`'s `esp_console_new_repl_uart()` binds to the *primary* console
(`CONFIG_ESP_CONSOLE_UART_DEFAULT`, physical UART0 on GPIO37/38). Typed input sent to
`/dev/cu.usbmodem101` never reached it - the REPL's own prompt/echo output appeared
fine (log output is mirrored to the *secondary* console,
`CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG`), but no keystroke was ever echoed back
or acted on, for either our own `ota_check`/`ota_update` commands or `esp_hosted`'s own
pre-existing `crash`/`reboot`/... commands (registered into the same global command
table, same symptom). `docs/hardware/jc4880p443c.md` already notes this board has two
USB-C ports "flashing/serial behavior can differ between them" - `/dev/cu.usbmodem101`
here is almost certainly the USB-Serial-JTAG (secondary, output-only-in-practice) path,
not whatever is wired to UART0's RX pin.

This does not affect the OTA mechanism itself (`ota_client`/`app_state_ota_task`,
exercised directly below) - only the interactive CLI's reachability over *this*
specific cable/port. Follow-up: confirm which of the two USB-C ports (or a real
UART-TTL adapter on GPIO37/38) makes the primary console interactive, or switch the
console binding to `esp_console_new_repl_usb_serial_jtag()` to match how this board is
actually being driven in practice.

## Step 2: real OTA flash from a published GitHub Release

Published `0.10.1` as a real public GitHub Release
(https://github.com/OleksandrDiachenko/esp32-market-data-terminal/releases/tag/0.10.1)
with `esp32-market-data-terminal.bin` as the asset - the first OTA-capable release.

Since the CLI trigger couldn't be exercised interactively (previous finding), a
temporary, one-shot task in `main/esp32-market-data-terminal.c` called
`ota_client_update_to("0.10.1")` directly 20s after boot, standing in for the console
command - same underlying call the `ota_update` command makes. Fully reverted after
this test (`git diff` clean before committing), same as
`docs/validation/websocket-streaming-hardware-test.md`'s temporary-hook precedent.

Board was running `0.10.0`. Result:

```text
W (25498) ota_test_hook: Forcing ota_client_update_to(0.10.1)
I (25498) ota_client: Starting OTA update to 0.10.1: https://github.com/.../releases/download/0.10.1/esp32-market-data-terminal.bin
E (25888) psa_crypto_driver_esp_ecdsa: ECDSA peripheral not supported on this chip revision
I (26848) esp-x509-crt-bundle: Certificate validated
I (28378) esp_https_ota: Starting OTA...
I (28378) esp_https_ota: Writing to <ota_1> partition at offset 0x420000
I (52748) ota_client: OTA succeeded, rebooting into 0.10.1
rst:0xc (SW_CPU_RESET),boot:0xc (SPI_FAST_FLASH_BOOT)
...
I (1178) app_init: App version:      0.10.1
```

Download+flash of the ~1.66 MB image took ~24s (28.4s -> 52.7s of device uptime),
written to `ota_1` as expected (the board was on `ota_0`). No `esp_ota_mark_app_valid_
cancel_rollback` warning on the new boot - confirmed silently `ESP_OK`, i.e. this OTA'd
image self-confirmed as valid.

The `psa_crypto_driver_esp_ecdsa: ECDSA peripheral not supported on this chip revision`
lines are a benign, expected side effect of this board's early-silicon P4 revision
(`CONFIG_ESP32P4_SELECTS_REV_LESS_V3`, `sdkconfig.defaults`) - mbedtls falls back to a
software ECDSA implementation and the TLS handshake still completes
("Certificate validated"), just logged at `E` level. Not a Phase 10 regression.

Note on OTA speed: this board has no native Wi-Fi - every packet of the ~1.66 MB
download also crosses the SDIO link to the ESP32-C6 co-processor
(`docs/decisions/0001-wifi-connectivity.md`), unlike a single-chip board (e.g.
ESP32-S3) where Wi-Fi data goes straight from radio to flash. `sdkconfig.defaults`
already documents a related tradeoff: RX streaming mode is kept off specifically
because it "triggered ESP-Hosted DMA heap fragmentation asserts during long HTTPS
transfers" - i.e. this project already trades some throughput for stability on exactly
this kind of transfer. A ~24s OTA download here vs. a native-Wi-Fi board is expected,
not a bug.

## Step 3: deliberate bad-image rollback test

Published `0.10.2` as a real GitHub Release
(https://github.com/OleksandrDiachenko/esp32-market-data-terminal/releases/tag/0.10.2),
clearly labeled in its release notes as an intentionally broken test image. Its
`esp32-market-data-terminal.bin` asset was built from a temporary local-only patch
(`abort()` as the first line of `app_main()`, before `esp_ota_mark_app_valid_cancel_
rollback()` - never committed) so it flashes and boots successfully (passing
`esp_https_ota`'s own image validation) but crashes before ever confirming itself
valid.

The same temporary test-hook task (now targeting `"0.10.2"`) was used to trigger the
update from the running, real `0.10.1`. Result - reproduced twice in a row:

```text
W (25496) ota_test_hook: Forcing ota_client_update_to(0.10.2)
I (25496) ota_client: Starting OTA update to 0.10.2: https://github.com/.../releases/download/0.10.2/esp32-market-data-terminal.bin
I (45256) ota_client: OTA succeeded, rebooting into 0.10.2
rst:0xc (SW_CPU_RESET),boot:0xc (SPI_FAST_FLASH_BOOT)
I (985) app_init: App version:      0.10.2
abort() was called at PC 0x48024b61 on core 0
...
Rebooting...
rst:0xc (SW_CPU_RESET),boot:0xc (SPI_FAST_FLASH_BOOT)
I (1174) app_init: App version:      0.10.1
```

The bad `0.10.2` image booted, crashed via `abort()` before reaching the mark-valid
call, and `CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT`'s default panic handler rebooted
automatically. On that next boot, the bootloader found `0.10.2`'s OTA state still
`PENDING_VERIFY` from the aborted attempt and reverted to the previously-confirmed-
valid `ota_0` (`0.10.1`) - no manual intervention, no stuck boot loop. The whole
download -> crash -> revert cycle repeated identically a second time (the rolled-
back-to `0.10.1` image still carried the same test hook, still targeting `"0.10.2"`),
confirming the behavior is deterministic, not a one-off.

## Final state

The temporary test-hook code (`main/esp32-market-data-terminal.c` `abort()` and
`ota_client_update_to()` call, `main/CMakeLists.txt`'s `ota_client` dependency) was
fully reverted (`git checkout --`) before committing anything. The board was reflashed
with the clean, real `0.10.1` build and confirmed booting normally with Wi-Fi/watchlist
intact:

```text
I (1130) app_init: App version:      0.10.1
I (4626) wifi_profile_store: Wi-Fi profile store ready (encrypted)
I (4636) app_state: Loaded 2 watchlist symbol(s)
```

The `0.10.2` GitHub Release is left published (clearly labeled as an intentionally
broken test image in its own description) as evidence of this test.
