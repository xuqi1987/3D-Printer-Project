idf_component_register(
    SRCS 
        "h264_rtsp_server.c"
    
    INCLUDE_DIRS 
        "include"
    
    REQUIRES 
        freertos
        esp_timer
        lwip
    
    PRIV_REQUIRES
        esp_netif
)