#pragma once
#include "Device.h"
#include <Adafruit_NeoPixel.h>

class Light : public Device
{
  public:
    Light(int pin, int numpixel)
        : mPixels{new Adafruit_NeoPixel(numpixel, pin, NEO_GRB + NEO_KHZ800)} {

          };

    virtual void loop();
    virtual void setup();

    void Red();
    void Green();
    void Blue();
    void Yellow();

    /**
     * @brief
     * @param  color            My Param doc
     * @param  wait             My Param doc
     */
    void colorWipe(uint32_t color, int wait = 1);

    /**
     * @brief
     * @param  color            My Param doc
     * @param  wait             My Param doc
     */
    void theaterChase(uint32_t color, int wait);

    /**
     * @brief
     * @param  wait             My Param doc
     */
    void rainbow(int wait);
    void rainbowCycle(uint8_t wait);
    void theaterChaseRainbow(int wait);

    /**
     * @brief
     * @param  WheelPos         My Param doc
     * @return uint32_t
     */
    uint32_t Wheel(byte WheelPos);

  private:
    Adafruit_NeoPixel *mPixels;
};