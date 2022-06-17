#define PRODUCT_KEY "a1KDUp02TuW"
#define DEVICE_NAME "Night_Lamp"
#define DEVICE_SECRET "40bc5b8283a098abde72d501f6edc85b"
#define REGION_ID "cn-shanghai"

#define WIFI_TIMEOUT 120 // seconds to run for

// Which pin on the Arduino is connected to the NeoPixels?
#define PIXEL_PIN        2 
#define TRIGGER_PIN      0
// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS 6 // Popular NeoPixel ring size

#define MAX_DEALY 20

typedef struct
{
    uint8 pixel_array[NUMPIXELS][3];
    uint8 brightness;
    uint8 mode;
    uint8 color_speed;
    uint8 powerstate;

}PixelsInfo;

PixelsInfo pixelsInfo = {
    .pixel_array = {   
        {255, 255, 255},
        {255, 255, 255},
        {255, 255, 255},
        {255, 255, 255},
        {255, 255, 255},
        {255, 255, 255}
    },
    .brightness = 50,
    .mode = 0,
    .color_speed = 1,
    .powerstate = 0,
};

