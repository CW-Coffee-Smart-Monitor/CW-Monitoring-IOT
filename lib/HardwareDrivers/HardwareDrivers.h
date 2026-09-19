#ifndef HARDWARE_DRIVERS_H
#define HARDWARE_DRIVERS_H

#include <Arduino.h>

class LedIndicator {
public:
    LedIndicator(uint8_t redPin, uint8_t greenPin, uint8_t bluePin);
    void begin();

    void setRGB(bool red, bool green, bool blue);
    void off();
    void red();
    void green();
    void blue();
    void yellow();
    void cyan();
    void purple();

    void blink(void (LedIndicator::*colorFunc)(), int times, int delayMs);
    void updateStatus(bool isCheckedIn, bool isReserved, bool isOccupied);

private:
    uint8_t rPin_;
    uint8_t gPin_;
    uint8_t bPin_;
};

class UltrasonicSensor {
public:
    UltrasonicSensor(uint8_t trigPin, uint8_t echoPin);
    void begin();

    // Returns distance in cm, or -1.0 if timeout/out of range
    float readDistanceCM(unsigned long timeoutUs = 30000);

private:
    uint8_t trigPin_;
    uint8_t echoPin_;
};

#endif  // HARDWARE_DRIVERS_H
