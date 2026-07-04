#include "market_data_ws_client.h"

#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "market_data_ws_stream_parser.h"
#include "market_data_ws_url.h"
#include "settings_store.h"

#define WS_URL_MAX 512
#define WS_STREAM_SUFFIX "kline_1s"
#define WS_BUFFER_SIZE 2048 // combined-stream kline events are ~300-400 bytes; generous headroom

static const char *TAG = "market_data_ws_client";

static esp_websocket_client_handle_t s_client;
static QueueHandle_t s_update_queue;

// Single connection, single event-handler task calling us serially - one
// in-flight message's parser state at a time is all that's needed.
static market_data_ws_stream_parser_t s_parser;
static market_data_kline_update_t s_pending_update;

// Re-read on every (re)connect rather than cached, same reasoning as
// market_data_client.c's select_base_url(): a region change without reboot
// must not silently keep using the stale host.
static const char *select_base_ws_url(void)
{
    api_region_settings_t region;
    settings_store_load_api_region(&region); // always ESP_OK, defaults substituted on corruption
    return region.region == SETTINGS_API_REGION_US ? "wss://stream.binance.us:9443"
                                                     : "wss://stream.binance.com:9443";
}

static void handle_data_event(const esp_websocket_event_data_t *data)
{
    if (data->op_code != 0x01 /* text */ && data->op_code != 0x00 /* continuation */)
    {
        return; // ping/pong/close frames are handled internally by esp_websocket_client
    }

    if (data->payload_offset == 0)
    {
        market_data_ws_stream_parser_init(&s_parser, &s_pending_update);
    }

    if (data->data_len > 0 &&
        market_data_ws_stream_parser_feed(&s_parser, data->data_ptr, (size_t)data->data_len) != MARKET_DATA_OK)
    {
        ESP_LOGW(TAG, "Malformed kline stream message; dropping");
        return;
    }

    if (data->payload_offset + data->data_len < data->payload_len)
    {
        return; // more chunks of this same message still to come
    }

    market_data_ws_parse_result_t result = market_data_ws_stream_parser_finish(&s_parser);
    if (result == MARKET_DATA_WS_PARSE_UPDATE)
    {
        if (xQueueSend(s_update_queue, &s_pending_update, 0) != pdTRUE)
        {
            ESP_LOGW(TAG, "Update queue full; dropping kline update for '%s'", s_pending_update.symbol);
        }
    }
    else if (result == MARKET_DATA_WS_PARSE_ERROR)
    {
        ESP_LOGW(TAG, "Failed to parse kline stream message");
    }
}

static void ws_event_handler(void *handler_arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_arg;
    (void)base;

    switch (event_id)
    {
    case WEBSOCKET_EVENT_DATA:
        handle_data_event((const esp_websocket_event_data_t *)event_data);
        break;
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WebSocket connected");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "WebSocket disconnected; esp_websocket_client will retry on its own");
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGW(TAG, "WebSocket error; esp_websocket_client will retry on its own");
        break;
    default:
        break;
    }
}

esp_err_t market_data_ws_client_start(const char *const *symbols, uint8_t symbol_count)
{
    if (symbols == NULL || symbol_count == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    s_update_queue = xQueueCreate(MARKET_DATA_WS_UPDATE_QUEUE_LEN, sizeof(market_data_kline_update_t));
    if (s_update_queue == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    char url[WS_URL_MAX];
    if (market_data_ws_url_build_combined_stream(select_base_ws_url(), symbols, symbol_count, WS_STREAM_SUFFIX, url,
                                                  sizeof(url)) != MARKET_DATA_OK)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_websocket_client_config_t config = {
        .uri = url, // strdup'd internally by esp_websocket_client_init(), safe to let url go out of scope
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = WS_BUFFER_SIZE,
        .disable_auto_reconnect = false, // reconnect-with-backoff is the library's own job, not hand-rolled here
    };

    s_client = esp_websocket_client_init(&config);
    if (s_client == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_websocket_register_events(s_client, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL);
    if (err != ESP_OK)
    {
        esp_websocket_client_destroy(s_client);
        s_client = NULL;
        return err;
    }

    return esp_websocket_client_start(s_client);
}

void market_data_ws_client_stop(void)
{
    if (s_client == NULL)
    {
        return;
    }
    esp_websocket_client_stop(s_client);
    esp_websocket_client_destroy(s_client);
    s_client = NULL;
}

QueueHandle_t market_data_ws_client_get_update_queue(void)
{
    return s_update_queue;
}
