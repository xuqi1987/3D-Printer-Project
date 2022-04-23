#include "Light.h"

void Light::loop()
{
}

void Light::setup()
{
    mPixels->begin();
    mPixels->setBrightness(100);
    mPixels->show();
}
void Light::Red()
{
    for (int i = 0; i < mPixels->numPixels(); i++)
    {
        mPixels->setPixelColor(i, mPixels->Color(255, 0, 0));
    }
    mPixels->show();
}
void Light::Green()
{
    for (int i = 0; i < mPixels->numPixels(); i++)
    {
        mPixels->setPixelColor(i, mPixels->Color(0, 255, 0));
    }
    mPixels->show();
}
void Light::Blue()
{
    for (int i = 0; i < mPixels->numPixels(); i++)
    {
        mPixels->setPixelColor(i, mPixels->Color(0, 0, 255));
    }
    mPixels->show();
}
void Light::Yellow()
{
    for (int i = 0; i < mPixels->numPixels(); i++)
    {
        mPixels->setPixelColor(i, mPixels->Color(255, 255, 0));
    }
    mPixels->show();
}

void Light::colorWipe(uint32_t color, int wait)
{
    for (int i = 0; i < mPixels->numPixels(); i++)
    {                                     // For each pixel in mPixels->..
        mPixels->setPixelColor(i, color); //  Set pixel's color (in RAM)
        mPixels->show();                  //  Update strip to match
        delay(wait);                      //  Pause for a moment
    }
}

void Light::theaterChase(uint32_t color, int wait)
{
    for (int a = 0; a < 10; a++)
    { // Repeat 10 times...
        for (int b = 0; b < 3; b++)
        {                     //  'b' counts from 0 to 2...
            mPixels->clear(); //   Set all pixels in RAM to 0 (off)
            // 'c' counts up from 'b' to end of strip in steps of 3...
            for (int c = b; c < mPixels->numPixels(); c += 3)
            {
                mPixels->setPixelColor(c, color); // Set pixel 'c' to value 'color'
            }
            mPixels->show(); // Update strip with new contents
            delay(wait);     // Pause for a moment
        }
    }
}

// Rainbow cycle along whole mPixels-> Pass delay time (in ms) between frames.
void Light::rainbow(int wait)
{
    // Hue of first pixel runs 3 complete loops through the color wheel.
    // Color wheel has a range of 65536 but it's OK if we roll over, so
    // just count from 0 to 3*65536. Adding 256 to firstPixelHue each time
    // means we'll make 3*65536/256 = 768 passes through this outer loop:
    for (long firstPixelHue = 0; firstPixelHue < 3 * 65536; firstPixelHue += 256)
    {
        for (int i = 0; i < mPixels->numPixels(); i++)
        { // For each pixel in mPixels->..
            // Offset pixel hue by an amount to make one full revolution of the
            // color wheel (range of 65536) along the length of the strip
            // (mPixels->numPixels() steps):
            int pixelHue = firstPixelHue + (i * 65536L / mPixels->numPixels());
            // mPixels->ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
            // optionally add saturation and value (brightness) (each 0 to 255).
            // Here we're using just the single-argument hue variant. The result
            // is passed through mPixels->gamma32() to provide 'truer' colors
            // before assigning to each pixel:
            mPixels->setPixelColor(i, mPixels->gamma32(mPixels->ColorHSV(pixelHue)));
        }
        mPixels->show(); // Update strip with new contents
        delay(wait);     // Pause for a moment
    }
}

void Light::rainbowCycle(uint8_t wait)
{
    uint16_t i, j;

    for (j = 0; j < 256 * 5; j++)
    { // 5 cycles of all colors on wheel
        for (i = 0; i < mPixels->numPixels(); i++)
        {
            mPixels->setPixelColor(i, Wheel(((i * 256 / mPixels->numPixels()) + j) & 255));
        }
        mPixels->show();
        delay(wait);
    }
}

void Light::theaterChaseRainbow(int wait)
{
    int firstPixelHue = 0; // First pixel starts at red (hue 0)
    for (int a = 0; a < 30; a++)
    { // Repeat 30 times...
        for (int b = 0; b < 3; b++)
        {                     //  'b' counts from 0 to 2...
            mPixels->clear(); //   Set all pixels in RAM to 0 (off)
            // 'c' counts up from 'b' to end of strip in increments of 3...
            for (int c = b; c < mPixels->numPixels(); c += 3)
            {
                // hue of pixel 'c' is offset by an amount to make one full
                // revolution of the color wheel (range 65536) along the length
                // of the strip (mPixels->numPixels() steps):
                int hue = firstPixelHue + c * 65536L / mPixels->numPixels();
                uint32_t color = mPixels->gamma32(mPixels->ColorHSV(hue)); // hue -> RGB
                mPixels->setPixelColor(c, color);                          // Set pixel 'c' to value 'color'
            }
            mPixels->show();             // Update strip with new contents
            delay(wait);                 // Pause for a moment
            firstPixelHue += 65536 / 90; // One cycle of color wheel over 90 frames
        }
    }
}

uint32_t Light::Wheel(byte WheelPos)
{
    WheelPos = 255 - WheelPos;
    if (WheelPos < 85)
    {
        return mPixels->Color(255 - WheelPos * 3, 0, WheelPos * 3);
    }
    if (WheelPos < 170)
    {
        WheelPos -= 85;
        return mPixels->Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
    WheelPos -= 170;
    return mPixels->Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}