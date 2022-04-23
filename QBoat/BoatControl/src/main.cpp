#include <Arduino.h>

#include <Servo.h>
Servo myservo; // create servo object to control a servo

void setup()
{
    // put your setup code here, to run once:
    Serial.begin(9600);
    myservo.attach(9); // attaches the servo on pin 9 to the servo object
    myservo.write(90);
}

void loop()
{

    // put your main code here, to run repeatedly:
    if (Serial.available() > 0)
    {
        if (Serial.read() == '\n')
        {

            int x = Serial.parseInt();
            int y = Serial.parseInt();
            int z = Serial.parseInt();

            int pos = map(y, 0, 255, 60, 120);
            int value = x - 128;
            if (value <= 0)
            {
                value = 0;
            }

            int speed = map(value, 0, 128, 0, 255);

            if (pos < 121 && pos > 59)
            {
                myservo.write(pos);
            }
            analogWrite(A0, speed);
        }
    }
    delay(10);
}