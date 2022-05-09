
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <AliyunIoTSDK.h>
#include <Adafruit_NeoPixel.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>  

#include "Lamp_config.hpp"

static WiFiClient espClient;
AliyunIoTSDK iot;
Adafruit_NeoPixel pixels(NUMPIXELS, PIXEL_PIN, NEO_GRB + NEO_KHZ800);
  


// Fill strip pixels one after another with a color. Strip is NOT cleared
// first; anything there will be covered pixel by pixel. Pass in color
// (as a single 'packed' 32-bit value, which you can get by calling
// strip.Color(red, green, blue) as shown in the loop() function above),
// and a delay time (in milliseconds) between pixels.
void colorWipe(uint32_t color, int wait) {
  for(int i=0; i<pixels.numPixels(); i++) { // For each pixel in strip...
    pixels.setPixelColor(i, color);         //  Set pixel's color (in RAM)
    pixels.show();                          //  Update strip to match
    delay(wait);                           //  Pause for a moment
  }
}

// Theater-marquee-style chasing lights. Pass in a color (32-bit value,
// a la strip.Color(r,g,b) as mentioned above), and a delay time (in ms)
// between frames.
void theaterChase(uint32_t color, int wait) {
  for(int a=0; a<10; a++) {  // Repeat 10 times...
    for(int b=0; b<3; b++) { //  'b' counts from 0 to 2...
      pixels.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in steps of 3...
      for(int c=b; c<pixels.numPixels(); c += 3) {
        pixels.setPixelColor(c, color); // Set pixel 'c' to value 'color'
      }
      pixels.show(); // Update strip with new contents
      delay(wait);  // Pause for a moment
    }
  }
}

// Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
void rainbow(int wait) {
  // Hue of first pixel runs 5 complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to 5*65536. Adding 256 to firstPixelHue each time
  // means we'll make 5*65536/256 = 1280 passes through this loop:
  for(long firstPixelHue = 0; firstPixelHue < 5*65536; firstPixelHue += 256) {
    // strip.rainbow() can take a single argument (first pixel hue) or
    // optionally a few extras: number of rainbow repetitions (default 1),
    // saturation and value (brightness) (both 0-255, similar to the
    // ColorHSV() function, default 255), and a true/false flag for whether
    // to apply gamma correction to provide 'truer' colors (default true).
    pixels.rainbow(firstPixelHue);
    // Above line is equivalent to:
    // strip.rainbow(firstPixelHue, 1, 255, 255, true);
    pixels.show(); // Update strip with new contents
    delay(wait);  // Pause for a moment
  }
}

// Rainbow-enhanced theater marquee. Pass delay time (in ms) between frames.
void theaterChaseRainbow(int wait) {
  int firstPixelHue = 0;     // First pixel starts at red (hue 0)
  for(int a=0; a<30; a++) {  // Repeat 30 times...
    for(int b=0; b<3; b++) { //  'b' counts from 0 to 2...
      pixels.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in increments of 3...
      for(int c=b; c<pixels.numPixels(); c += 3) {
        // hue of pixel 'c' is offset by an amount to make one full
        // revolution of the color wheel (range 65536) along the length
        // of the strip (strip.numPixels() steps):
        int      hue   = firstPixelHue + c * 65536L / pixels.numPixels();
        uint32_t color = pixels.gamma32(pixels.ColorHSV(hue)); // hue -> RGB
        pixels.setPixelColor(c, color); // Set pixel 'c' to value 'color'
      }
      pixels.show();                // Update strip with new contents
      delay(wait);                 // Pause for a moment
      firstPixelHue += 65536 / 90; // One cycle of color wheel over 90 frames
    }
  }
}

void update_pixels_mode()
{
    switch (pixelsInfo.mode)
    {
        case 2:
            rainbow(pixelsInfo.color_speed * MAX_DEALY);
        break;
        case 3:
            theaterChase(pixels.Color(pixelsInfo.pixel_array[0][0],pixelsInfo.pixel_array[0][1],pixelsInfo.pixel_array[0][2]),pixelsInfo.color_speed * MAX_DEALY);
        break;
        case 4:
            theaterChaseRainbow(pixelsInfo.color_speed * MAX_DEALY);
        break;
        default:
            colorWipe(pixels.Color(pixelsInfo.pixel_array[0][0],pixelsInfo.pixel_array[0][1],pixelsInfo.pixel_array[0][2]),pixelsInfo.color_speed * MAX_DEALY);
        break;
    }
}

void update_pixels()
{
    pixels.clear();
    if (pixelsInfo.powerstate == 1)
    {
        for (int i = 0; i < NUMPIXELS; i++)
        {
            pixels.setPixelColor(i, pixelsInfo.pixel_array[i][0], pixelsInfo.pixel_array[i][1], pixelsInfo.pixel_array[i][2]);
        }
    }
    else
    {
        for (int i = 0; i < NUMPIXELS; i++)
        {
            pixels.setPixelColor(i, 0, 0, 0);
        }
    }
    pixels.setBrightness(pixelsInfo.brightness);
    pixels.show();   // Send the updated pixel colors to the hardware.
}


void RGBColor(JsonVariant p)
{
    for(int i=0; i<NUMPIXELS; i++) { // For each pixel...
        // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
        // Here we're using a moderately bright green color:
        pixelsInfo.pixel_array[i][0] = p["RGBColor"]["Red"].as<int>();
        pixelsInfo.pixel_array[i][1] = p["RGBColor"]["Green"].as<int>();
        pixelsInfo.pixel_array[i][2] = p["RGBColor"]["Blue"].as<int>();
    }
    update_pixels();
}

void powerstate(JsonVariant p)
{
    pixelsInfo.powerstate = p["powerstate"].as<bool>();
    update_pixels();
}

void brightness(JsonVariant p)
{
    pixelsInfo.brightness = p["brightness"].as<int>();
    update_pixels();
}

void ColorSpeed(JsonVariant p)
{
    pixelsInfo.color_speed = p["ColorSpeed"].as<int>();
    update_pixels_mode();
}
void mode(JsonVariant p)
{
    pixelsInfo.mode = p["mode"].as<int>();
    update_pixels_mode();
}

void wifiInit()
{
    
    WiFiManager wifiManager;  
    wifiManager.setConfigPortalTimeout(WIFI_TIMEOUT);
    if (!wifiManager.autoConnect("AutoConnectAP")) 
    {
        Serial.println("failed to connect and hit timeout");
        delay(3000);
        //reset and try again, or maybe put it to deep sleep
        ESP.restart();
        delay(5000);
    }
    Serial.println("Connected to AP");
    
}

void setup()
{
    Serial.begin(9600);

    wifiInit();

    AliyunIoTSDK::begin(espClient, PRODUCT_KEY, DEVICE_NAME, DEVICE_SECRET, REGION_ID);

    // 绑定属性回调
    AliyunIoTSDK::bindData("RGBColor", RGBColor);
    AliyunIoTSDK::bindData("powerstate", powerstate);
    AliyunIoTSDK::bindData("brightness", brightness);
    AliyunIoTSDK::bindData("ColorSpeed", ColorSpeed);
    AliyunIoTSDK::bindData("mode", mode);

    pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
    AliyunIoTSDK::send("powerstate", pixelsInfo.powerstate);
    AliyunIoTSDK::send("brightness", pixelsInfo.brightness);
    AliyunIoTSDK::send("ColorSpeed", pixelsInfo.color_speed);
    AliyunIoTSDK::send("mode", pixelsInfo.mode);
    char buffer[100] = "";
    sprintf(buffer,"{\"RGBColor\":{\"Red\":%d,\"Green\":%d,\"Blue\":%d}}",pixelsInfo.pixel_array[0][0],pixelsInfo.pixel_array[0][1],pixelsInfo.pixel_array[0][2]);
    AliyunIoTSDK::send("RGBColor", buffer);
    update_pixels();
}

void loop()
{
    AliyunIoTSDK::loop();
}


