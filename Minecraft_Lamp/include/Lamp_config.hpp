#define PRODUCT_KEY "a1ZSfU0pulo"
#define DEVICE_NAME "Minecraft_Lamp"
#define DEVICE_SECRET "1fe509aade8c7fb01f49d48aa314fe53"
#define REGION_ID "cn-shanghai"

#define WIFI_TIMEOUT 120 // seconds to run for

// Which pin on the Arduino is connected to the NeoPixels?
#define PIXEL_PIN        D2 
#define TRIGGER_PIN      D0
// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS 12 // Popular NeoPixel ring size

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
        {255, 255, 255},
        {255, 255, 255},
        {255, 255, 255},
        {255, 255, 255},
        {255, 255, 255},
        {255, 255, 255},
        {255, 255, 255},
    },
    .brightness = 50,
    .mode = 0,
    .color_speed = 1,
    .powerstate = 0,
};

