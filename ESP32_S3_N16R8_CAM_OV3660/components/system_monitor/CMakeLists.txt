idf_component_register(
    SRCS 
        "system_monitor.c"
    
    INCLUDE_DIRS 
        "include"
    
    REQUIRES 
        freertos
        esp_timer
        heap
    
    PRIV_REQUIRES
        esp_system
        esp_psram
)