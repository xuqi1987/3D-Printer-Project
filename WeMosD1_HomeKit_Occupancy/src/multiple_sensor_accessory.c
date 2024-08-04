
#include <homekit/homekit.h>
#include <homekit/characteristics.h>

// Called to identify this accessory. See HAP section 6.7.6 Identify Routine
// Generally this is called when paired successfully or click the "Identify Accessory" button in Home APP.
void multiple_sensor_accessory_identify(homekit_value_t _value) {
	printf("accessory identify\n");
}

// format: uint8; 0 ”Occupancy is not detected”, 1 ”Occupancy is detected”
homekit_characteristic_t occupancy_cha_occupancy = HOMEKIT_CHARACTERISTIC_(OCCUPANCY_DETECTED, 0);

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_bridge, .services=(homekit_service_t*[]) {
    	// HAP section 8.17:
    	// For a bridge accessory, only the primary HAP accessory object must contain this(INFORMATION) service.
    	// But in my test,
    	// the bridged accessories must contain an INFORMATION service,
    	// otherwise the HomeKit will reject to pair.
    	HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Multiple Sensors"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Arduino HomeKit"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0123456"),
            HOMEKIT_CHARACTERISTIC(MODEL, "ESP8266/ESP32"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, multiple_sensor_accessory_identify),
            NULL
        }),
        NULL
    }),
	HOMEKIT_ACCESSORY(.id=2, .category=homekit_accessory_category_sensor, .services=(homekit_service_t*[]) {
    	HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
			HOMEKIT_CHARACTERISTIC(NAME, "Occupancy Sensor"),
			HOMEKIT_CHARACTERISTIC(IDENTIFY, multiple_sensor_accessory_identify),
			NULL
		}),
    	HOMEKIT_SERVICE(OCCUPANCY_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
			HOMEKIT_CHARACTERISTIC(NAME, "Occupancy"),
			&occupancy_cha_occupancy,
			NULL
		}),
		NULL
	}),
    NULL
};

homekit_server_config_t multiple_sensor_config = {
		.accessories = accessories,
		.password = "111-11-111"
};


