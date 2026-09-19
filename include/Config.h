#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =====================
// KONFIGURASI MEJA & LOGIKA
// =====================
#define TABLE_ID 12
#define OCCUPIED_DISTANCE_CM 20.0f

// Timeout auto check-out jika meja kosong terus (ms)
// Demo: 15000 (15 detik), Final: 15 * 60 * 1000 (15 menit)
#define AUTO_CHECKOUT_TIMEOUT 15000

// =====================
// KONFIGURASI JARINGAN & SERVER
// =====================
#define WIFI_SSID "WAWAN"
#define WIFI_PASSWORD "wawannnn"

#define SOCKET_HOST "152.42.207.49"
#define SOCKET_PORT 9001

#define API_BASE_URL "http://152.42.207.49:9002"
#define WEBHOOK_URL  "https://fasilita.my.id/api/webhook"

// =====================
// PIN RFID RC522 (SPI)
// =====================
#define SS_PIN   5
#define RST_PIN  22

// =====================
// PIN ULTRASONIK HC-SR04
// =====================
#define TRIG_PIN 26
#define ECHO_PIN 27

// =====================
// PIN LED RGB COMMON ANODE
// R: GPIO 2, G: GPIO 4, B: GPIO 21, Common: 3V3
// =====================
#define LED_R_PIN 2
#define LED_G_PIN 4
#define LED_B_PIN 21

// =====================
// TIMING INTERVALS (ms)
// =====================
#define RFID_COOLDOWN_MS   2000
#define MONITOR_INTERVAL_MS 2000
#define API_INTERVAL_MS     5000

#endif // CONFIG_H
