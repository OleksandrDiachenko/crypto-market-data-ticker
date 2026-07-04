#pragma once

// Orchestrates app_state's live WebSocket updates: starts
// market_data_ws_client for the current watchlist and spawns the sole
// consumer task for its update queue, which applies each `@kline_1s` update
// via app_state_apply_kline_update(). Separate from app_state_sync_task -
// that task blocks on REST HTTP calls for seconds at a time, and folding
// this queue's low-latency consumption into it would stall live updates
// behind REST calls.
//
// Soft dependency like app_state_sync_task_start(): failure here only
// costs live updates between REST syncs, never fatal to the rest of the app.

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Call once, after app_state_init() has loaded the watchlist (and ideally
// after at least Wi-Fi has been started - the WebSocket client tolerates
// not-yet-connected the same way market_data_client's REST calls do).
esp_err_t app_state_ws_task_start(void);

#ifdef __cplusplus
}
#endif
