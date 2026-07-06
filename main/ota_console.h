#pragma once

// Registers "ota_check"/"ota_update" console commands and starts a REPL
// over USB-Serial-JTAG - the CLI/log-driven manual OTA trigger Phase 10's
// roadmap scope calls for until Phase 11's Settings screen exists. Shares
// the same USB-Serial-JTAG port already used for log output and flashing
// (CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG), the same way ESP-IDF's own console
// examples do - no separate wiring needed to use it over `idf.py monitor`
// or a plain serial connection to /dev/cu.usbmodem101.

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Call once, after app_state_ota_task_start().
esp_err_t ota_console_start(void);

#ifdef __cplusplus
}
#endif
