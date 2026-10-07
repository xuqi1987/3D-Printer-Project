#include "http_server.h"

#include <string.h>
#include "esp_log.h"
#include "wifi.h"

static const char *TAG = "HTTP";

static power_action_cb s_short_cb = NULL;
static power_action_cb s_long_cb  = NULL;

static const char kIndexHtml[] =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>PC Power</title>"
    "<style>"
    "body{font-family:-apple-system,sans-serif;text-align:center;margin-top:60px;background:#1e1e1e;color:#eee;}"
    "h1{color:#4CAF50;}"
    ".btn{display:inline-block;padding:24px 48px;margin:12px;font-size:20px;"
    "border:none;border-radius:10px;color:#fff;cursor:pointer;text-decoration:none;}"
    ".btn-on{background:#4CAF50;}.btn-on:hover{background:#45a049;}"
    ".btn-off{background:#f44336;}.btn-off:hover{background:#da190b;}"
    ".status{margin-top:30px;font-size:14px;color:#888;}"
    "</style></head><body>"
    "<h1>PC Power Control</h1>"
    "<p><a href='/power?action=press' class='btn btn-on'>POWER ON / WAKE</a></p>"
    "<p><a href='/power?action=long' class='btn btn-off'>FORCE OFF (5s)</a></p>"
    "<p class='status' id='status'>loading...</p>"
    "<script>"
    "fetch('/status').then(r=>r.json()).then(s=>{"
    "  document.getElementById('status').textContent="
    "    'WiFi: ' + (s.wifi?'connected':'disconnected') + ' | IP: ' + s.ip;"
    "}).catch(()=>{});"
    "</script>"
    "</body></html>";

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, kIndexHtml, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_handler(httpd_req_t *req)
{
    char body[160];
    snprintf(body, sizeof(body),
             "{\"wifi\":%s,\"ip\":\"%s\"}",
             wifi_is_connected() ? "true" : "false",
             wifi_is_connected() ? "connected" : "0.0.0.0");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t power_handler(httpd_req_t *req)
{
    char query[64] = {0};
    char action[16] = "press";

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "action", action, sizeof(action));
    }

    ESP_LOGI(TAG, "action=%s", action);

    if (strcmp(action, "long") == 0) {
        if (s_long_cb) s_long_cb();
    } else {
        if (s_short_cb) s_short_cb();
    }

    char body[96];
    snprintf(body, sizeof(body), "{\"ok\":true,\"action\":\"%s\"}", action);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

httpd_handle_t http_server_start(power_action_cb short_cb, power_action_cb long_cb)
{
    s_short_cb = short_cb;
    s_long_cb  = long_cb;

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return NULL;
    }

    httpd_uri_t root   = { .uri = "/",       .method = HTTP_GET, .handler = root_handler  };
    httpd_uri_t power  = { .uri = "/power",  .method = HTTP_GET, .handler = power_handler };
    httpd_uri_t status = { .uri = "/status", .method = HTTP_GET, .handler = status_handler };
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &power);
    httpd_register_uri_handler(server, &status);

    ESP_LOGI(TAG, "HTTP server started on port %d", cfg.server_port);
    return server;
}
