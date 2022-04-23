#include <Arduino.h>

int joystick[3] = {512, 512, 1};
int X_PIN = A0;
int Y_PIN = A1;
int Z_PIN = 7;
bool Speed_lock = false;

void Send(int array[], size_t len)
{
    Serial.print(array[0]);
    Serial.print(",");
    Serial.print(array[1]);
    Serial.print(",");
    Serial.print(array[2]);
    Serial.println();
}
void setup()
{
    // initialize serial communication at 9600 bits per second:
    Serial.begin(9600);
    pinMode(X_PIN, INPUT);
    pinMode(Y_PIN, INPUT);
    pinMode(Z_PIN, INPUT);
    pinMode(LED_BUILTIN, OUTPUT);
}

// the loop routine runs over and over again forever:
void loop()
{
    int speed = map(analogRead(X_PIN), 0, 1023, 0, 255);
    int dir = map(analogRead(Y_PIN), 0, 1023, 0, 255);

    int z = digitalRead(7);

    if (z == LOW)
    {
        Speed_lock = !Speed_lock;
    }

    if (!Speed_lock)
    {
        joystick[0] = speed;
    }

    joystick[1] = dir;
    joystick[2] = z;

    Send(joystick, sizeof(joystick));

    if (Speed_lock)
    {
        digitalWrite(LED_BUILTIN, HIGH);
    }
    else
    {
        digitalWrite(LED_BUILTIN, LOW);
    }

    delay(500); // delay in between reads for stability
}