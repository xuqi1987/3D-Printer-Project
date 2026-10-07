#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_http_server.h"

/**
 * @brief Callback type for power actions invoked by HTTP handlers.
 */
typedef void (*power_action_cb)(void);

/**
 * @brief Start the HTTP server with / and /power endpoints.
 *
 * @param short_cb  invoked when a short press is requested
 * @param long_cb   invoked when a long press (force off) is requested
 * @return          HTTP server handle, or NULL on failure
 */
httpd_handle_t http_server_start(power_action_cb short_cb, power_action_cb long_cb);

#endif // HTTP_SERVER_H
