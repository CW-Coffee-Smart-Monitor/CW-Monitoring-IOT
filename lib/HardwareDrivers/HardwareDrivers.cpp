#include "HardwareDrivers.h"

// ==========================================
// LedIndicator Implementation (Common Anode)
// LOW = Active/ON, HIGH = Inactive/OFF
// ==========================================
LedIndicator::LedIndicator(uint8_t redPin, uint8_t greenPin, uint8_t bluePin)
    : rPin_(redPin), gPin_(greenPin), bPin_(bluePin) {}

void LedIndicator::begin() {
    pinMode(rPin_, OUTPUT);
    pinMode(gPin_, OUTPUT);
    pinMode(bPin_, OUTPUT);
    off();
}

void LedIndicator::setRGB(bool red, bool green, bool blue) {
    digitalWrite(rPin_, red ? LOW : HIGH);
    digitalWrite(gPin_, green ? LOW : HIGH);
    digitalWrite(bPin_, blue ? LOW : HIGH);
}

void LedIndicator::off() {
    setRGB(true, true, true);
}
void LedIndicator::red() {
    setRGB(false, true, true);
}
void LedIndicator::green() {
    setRGB(true, false, true);
}
void LedIndicator::blue() {
    setRGB(true, true, false);
}
void LedIndicator::yellow() {
    setRGB(false, false, true);
}
void LedIndicator::cyan() {
    setRGB(true, false, false);
}
void LedIndicator::purple() {
    setRGB(false, true, false);
}

void LedIndicator::blink(void (LedIndicator::*colorFunc)(), int times, int delayMs) {
    for (int i = 0; i < times; i++) {
        (this->*colorFunc)();
        delay(delayMs);
        off();
        delay(delayMs);
    }
}

void LedIndicator::updateStatus(bool isCheckedIn, bool isReserved, bool isOccupied) {
    if (isCheckedIn) {
        green();  // Sedang digunakan
    } else if (isReserved) {
        cyan();  // Meja direservasi
    } else if (isOccupied) {
        yellow();  // Ada orang tapi belum check-in
    } else {
        blue();  // Kosong / tersedia
    }
}

// ==========================================
// UltrasonicSensor Implementation (HC-SR04)
// ==========================================
UltrasonicSensor::UltrasonicSensor(uint8_t trigPin, uint8_t echoPin) : trigPin_(trigPin), echoPin_(echoPin) {}

void UltrasonicSensor::begin() {
    pinMode(trigPin_, OUTPUT);
    pinMode(echoPin_, INPUT);
}

float UltrasonicSensor::readDistanceCM(unsigned long timeoutUs) {
    digitalWrite(trigPin_, LOW);
    delayMicroseconds(2);

    digitalWrite(trigPin_, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin_, LOW);

    long durasi = pulseIn(echoPin_, HIGH, timeoutUs);

    if (durasi == 0) {
        return -1.0f;
    }

    return (durasi * 0.0343f / 2.0f);
}
