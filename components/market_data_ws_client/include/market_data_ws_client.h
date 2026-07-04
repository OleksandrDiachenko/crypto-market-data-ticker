#pragma once

// Public API for the Binance public WebSocket kline stream client. Owns one
// combined-stream connection (`{sym0}@kline_1s/{sym1}@kline_1s/...`) for the
// whole watchlist and a FreeRTOS queue of decoded market_data_kline_update_t
// - the same owner-owns-queue, single-consumer pattern wifi_manager already
// uses for its own event queue (see wifi_manager.h's
// wifi_manager_get_event_queue()). No API keys, no trading logic.
//
// Region-aware endpoint selection reuses settings_store_load_api_region(),
// mirroring market_data_client.c's select_base_url(). Reconnect-with-backoff
// is esp_websocket_client's own built-in behavior, not hand-rolled here.

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "market_data_kline_update.h"

#ifdef __cplusplus
extern "C" {
#endif

// Depth sized for 8 watchlist symbols emitting ~1 update/s each, consumed
// near-instantly by app_state_ws_task - generous headroom, not a tight fit.
#define MARKET_DATA_WS_UPDATE_QUEUE_LEN 32

// Builds the region-aware combined-stream URL for symbols[0..symbol_count)
// and connects (auto-reconnect-with-backoff enabled). symbols are copied
// internally (do not need to outlive this call). symbol_count must be
// 1..SETTINGS_MAX_WATCHLIST.
//
// Soft dependency like Wi-Fi/time_sync: returns ESP_OK once the client is
// created and started, even if the initial connect attempt later fails -
// esp_websocket_client keeps retrying on its own. Only ESP_ERR_INVALID_ARG /
// ESP_ERR_NO_MEM (bad arguments, allocation failure) are returned as errors.
esp_err_t market_data_ws_client_start(const char *const *symbols, uint8_t symbol_count);

// Stops and destroys the underlying client and its queue. Not currently
// called anywhere (no runtime watchlist-edit UI exists yet) - exposed for
// symmetry and future use.
void market_data_ws_client_stop(void);

// Sole consumer: components/app_state/src/app_state_ws_task.c. Returns NULL
// if market_data_ws_client_start() was never called (or failed).
QueueHandle_t market_data_ws_client_get_update_queue(void);

#ifdef __cplusplus
}
#endif
