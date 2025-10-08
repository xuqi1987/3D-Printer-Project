idf_component_register(
    SRCS 
        "main.c"
    
    INCLUDE_DIRS 
        "."
    
    REQUIRES 
        camera_manager
        wifi_manager
        mjpeg_http_server
        system_monitor
        nvs_flash
        esp_timer
        freertos
    
    PRIV_REQUIRES
        driver
        esp_psram
        esp_hw_support
        spi_flash
)
