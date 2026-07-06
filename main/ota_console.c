#include "ota_console.h"

#include "app_state_ota_task.h"
#include "esp_console.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "ota_console";

static int cmd_ota_check(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    app_state_ota_check_now();
    printf("OTA check requested - see logs for the result.\n");
    return 0;
}

static int cmd_ota_update(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    app_state_ota_update_now();
    printf("OTA update requested (only runs if a check found one available) - device reboots on success.\n");
    return 0;
}

esp_err_t ota_console_start(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "ota>";
#if CONFIG_DEV_SCREENSHOT_CONSOLE
    // lv_snapshot_take_to_draw_buf() (main/dev_screenshot_console.c) runs
    // LVGL's full render pipeline synchronously on this REPL task's own
    // stack - the same recursive obj_refr/draw_rect/border-drawing chain
    // the LVGL port's own render task runs with a 7168-byte stack
    // (ESP_LVGL_PORT_INIT_CONFIG() in esp_lvgl_port.h). The default
    // 4096-byte REPL stack overflowed here on real hardware ("Stack
    // protection fault" in task "console_repl").
    repl_config.task_stack_size = 12288;
#endif

    esp_console_dev_usb_serial_jtag_config_t jtag_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    esp_err_t err = esp_console_new_repl_usb_serial_jtag(&jtag_config, &repl_config, &repl);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create console REPL: %s", esp_err_to_name(err));
        return err;
    }

    const esp_console_cmd_t ota_check_cmd = {
        .command = "ota_check",
        .help = "Check GitHub Releases for a new firmware version now",
        .hint = NULL,
        .func = &cmd_ota_check,
    };
    const esp_console_cmd_t ota_update_cmd = {
        .command = "ota_update",
        .help = "Flash the latest release if a check has found one available",
        .hint = NULL,
        .func = &cmd_ota_update,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&ota_check_cmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&ota_update_cmd));

    return esp_console_start_repl(repl);
}
