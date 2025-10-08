idf_component_register(
    SRCS "mjpeg_http_server.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server esp_timer
)