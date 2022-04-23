#include "ButtonDebounce.h"
#include "Light.h"
#include <Arduino.h>

#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`
#include <Wire.h>        // Only needed for Arduino 1.6.5 and earlier

#include "Keyboard.h"

/** Global value **/
ButtonDebounce btn3(D8, INPUT, LOW);
ButtonDebounce btn4(D7, INPUT, LOW);
ButtonDebounce btn2(D6, INPUT, LOW);
ButtonDebounce btn1(D5, INPUT, LOW);

Light light(D4, 8);

SSD1306Wire display(0x3c, SDA, SCL);

int clickBtn = 0;

/**
 * @brief
 */
void drawLines()
{
    for (int16_t i = 0; i < display.getWidth(); i += 4)
    {
        display.drawLine(0, 0, i, display.getHeight() - 1);
        display.display();
        delay(10);
    }
    for (int16_t i = 0; i < display.getHeight(); i += 4)
    {
        display.drawLine(0, 0, display.getWidth() - 1, i);
        display.display();
        delay(10);
    }
    delay(250);

    display.clear();
    for (int16_t i = 0; i < display.getWidth(); i += 4)
    {
        display.drawLine(0, display.getHeight() - 1, i, 0);
        display.display();
        delay(10);
    }
    for (int16_t i = display.getHeight() - 1; i >= 0; i -= 4)
    {
        display.drawLine(0, display.getHeight() - 1, display.getWidth() - 1, i);
        display.display();
        delay(10);
    }
    delay(250);

    display.clear();
    for (int16_t i = display.getWidth() - 1; i >= 0; i -= 4)
    {
        display.drawLine(display.getWidth() - 1, display.getHeight() - 1, i, 0);
        display.display();
        delay(10);
    }
    for (int16_t i = display.getHeight() - 1; i >= 0; i -= 4)
    {
        display.drawLine(display.getWidth() - 1, display.getHeight() - 1, 0, i);
        display.display();
        delay(10);
    }
    delay(250);
    display.clear();
    for (int16_t i = 0; i < display.getHeight(); i += 4)
    {
        display.drawLine(display.getWidth() - 1, 0, 0, i);
        display.display();
        delay(10);
    }
    for (int16_t i = 0; i < display.getWidth(); i += 4)
    {
        display.drawLine(display.getWidth() - 1, 0, i, display.getHeight() - 1);
        display.display();
        delay(10);
    }
    delay(250);
}
void drawRect(void)
{
    for (int16_t i = 0; i < display.getHeight() / 2; i += 2)
    {
        display.drawRect(i, i, display.getWidth() - 2 * i, display.getHeight() - 2 * i);
        display.display();
        delay(10);
    }
}
void fillRect(void)
{
    uint8_t color = 1;
    for (int16_t i = 0; i < display.getHeight() / 2; i += 3)
    {
        display.setColor((color % 2 == 0) ? BLACK : WHITE); // alternate colors
        display.fillRect(i, i, display.getWidth() - i * 2, display.getHeight() - i * 2);
        display.display();
        delay(10);
        color++;
    }
    // Reset back to WHITE
    display.setColor(WHITE);
}
// Adapted from Adafruit_SSD1306
void drawCircle(void)
{
    for (int16_t i = 0; i < display.getHeight(); i += 2)
    {
        display.drawCircle(display.getWidth() / 2, display.getHeight() / 2, i);
        display.display();
        delay(10);
    }
    delay(1000);
    display.clear();

    // This will draw the part of the circel in quadrant 1
    // Quadrants are numberd like this:
    //   0010 | 0001
    //  ------|-----
    //   0100 | 1000
    //
    display.drawCircleQuads(display.getWidth() / 2, display.getHeight() / 2, display.getHeight() / 4, 0b00000001);
    display.display();
    delay(200);
    display.drawCircleQuads(display.getWidth() / 2, display.getHeight() / 2, display.getHeight() / 4, 0b00000011);
    display.display();
    delay(200);
    display.drawCircleQuads(display.getWidth() / 2, display.getHeight() / 2, display.getHeight() / 4, 0b00000111);
    display.display();
    delay(200);
    display.drawCircleQuads(display.getWidth() / 2, display.getHeight() / 2, display.getHeight() / 4, 0b00001111);
    display.display();
}

void button_setup()
{

    btn4.setClickCallback([&](int pin) {
        light.Blue();
        Serial.println(pin);
        clickBtn = pin;
        Keyboard.print("You pressed the button ");
    });
    btn3.setClickCallback([&](int pin) {
        Serial.println(pin);
        light.Green();
        clickBtn = pin;
    });
    btn2.setClickCallback([&](int pin) {
        light.Red();
        Serial.println(pin);
        clickBtn = pin;
    });
    btn1.setClickCallback([&](int pin) {
        light.Yellow();
        Serial.println(pin);
        clickBtn = pin;
    });
}

void light_setup()
{
    light.setup();
}

void setup()
{
    // put your setup code here, to run once:
    Serial.begin(9600);
    Serial.println("Hello World!");

    button_setup();
    light_setup();
    light.Red();

    display.init();
    display.setContrast(255);
    Keyboard.begin();
}

void loop()
{
    if (clickBtn == D8)
    {
        fillRect();
        clickBtn = 0;
        display.clear();
    }
    else if (clickBtn == D7)
    {
        drawRect();
        clickBtn = 0;
        display.clear();
    }
    else if (clickBtn == D6)
    {
        drawCircle();
        clickBtn = 0;
        display.clear();
    }
    else if (clickBtn == D5)
    {
        drawLines();
        clickBtn = 0;
        display.clear();
    }
}