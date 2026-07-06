# Hardware validation: primary console rebind to USB-Serial-JTAG

## Environment

- Date: 2026-07-06
- Board: JC4880P443C_I_W
- Target: esp32p4, ESP-IDF v6.0.1
- Port: /dev/cu.usbmodem101

## Background

`docs/validation/ota-firmware-update-hardware-test.md` found that `/dev/cu.usbmodem101`
only carried log/stdout output - typed input never reached the `ota_check`/`ota_update`
REPL, since the primary console (which reads stdin) was bound to UART0
(`esp_console_new_repl_uart()`), a different physical path from the USB-Serial-JTAG
port that `/dev/cu.usbmodem101` actually is. That report already concluded the fix:
make USB-Serial-JTAG the primary console instead of secondary
(`esp_console_new_repl_usb_serial_jtag()`).

This is a prerequisite for a planned dev-only "screenshot over serial" feature (Claude
Code sends a command over serial, firmware replies with a captured LVGL screenshot) -
without a working input path, no command can ever reach the device from this port.

## Change

- `sdkconfig.defaults`: `CONFIG_ESP_CONSOLE_UART_DEFAULT` off, `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` on (primary role). Kconfig's `choice` logic automatically forced `CONFIG_ESP_CONSOLE_SECONDARY_NONE=y` and cleared `CONFIG_ESP_CONSOLE_UART`/`_NUM` as a result - no manual secondary-console toggle needed.
- `main/ota_console.c`: `esp_console_new_repl_uart()` + `esp_console_dev_uart_config_t` replaced with `esp_console_new_repl_usb_serial_jtag()` + `esp_console_dev_usb_serial_jtag_config_t`. Everything else (command registration, `esp_console_start_repl`) unchanged.
- A temporary debug label (`s_debug_label` in `main/display_ui.c`'s status bar, wired via a temporary `display_ui_debug_set_last_command()` helper called from `cmd_ota_check`) was added purely to get a visual, on-device confirmation that input actually reached the firmware, not just that the serial reply looked right. Reverted after this test.

## Method

Flashed via `idf.py -p /dev/cu.usbmodem101 flash` (ESP-IDF v6.0.1 - the version this
project's `build/` directory is configured against; a mismatched `~/esp/esp-idf-v6.0.2`
install was tried first and failed with a CMake bootloader-subproject source mismatch,
unrelated to this change). Then, since an interactive TTY wasn't available in this
session, a `pyserial` script wrote `ota_check\n` to `/dev/cu.usbmodem101` and read back
the reply (same pyserial-instead-of-`idf.py monitor` approach as the original OTA
hardware test).

## Result

```text
>> ota_check
>> OTA check requested - see logs for the result.
>> ota>
```

The REPL echoed the typed command and executed it - both the echo and the response
text prove input is now actually being read from this port (previously nothing was).
The user separately confirmed the physical device's status bar showed the temporary
debug label's text, `cmd: ota_check`, next to the Wi-Fi indicator - visual,
on-hardware confirmation that the same command that reached the console also drove a
real UI update, not just a serial-level echo.

## Conclusion

The rebind works as intended. `ota_check`/`ota_update` (and any future console
command, e.g. the planned `screenshot` command) are now reachable from
`/dev/cu.usbmodem101`, the same port already used for flashing and logs. The temporary
debug label and its wiring in `ota_console.c`/`display_ui.c`/`display_ui.h` were
reverted after this test; only the console-rebind change itself
(`sdkconfig.defaults`, `main/ota_console.c`) is kept.
