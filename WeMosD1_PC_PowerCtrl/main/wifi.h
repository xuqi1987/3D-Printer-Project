#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>

/**
 * @brief Initialize WiFi in station mode and connect to the configured AP.
 *        Non-blocking — connection state is monitored via events.
 */
void wifi_init_sta(void);

/**
 * @brief Check if currently connected to the WiFi AP and has an IP.
 */
bool wifi_is_connected(void);

#endif // WIFI_H
