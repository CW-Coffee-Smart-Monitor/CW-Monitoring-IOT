#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ArduinoJson.h>
#include <WiFi.h>
// #include <PubSubClient.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// =====================
// KIRIM WEBHOOK
// =====================

void sendToServer(JsonDocument& doc) {
  if (WiFi.status() == WL_CONNECTED) {

    WiFiClientSecure client;
    client.setInsecure(); // Disable certificate validation (not recommended for production)

    HTTPClient http;
    http.begin(client, "https://cw-monitoring.vercel.app/api/webhook");

    http.addHeader("Content-Type", "application/json");

    String jsonString;
    serializeJson(doc, jsonString);

    int responseCode = http.POST(jsonString);

    Serial.print("HTTP Response: ");
    Serial.println(responseCode);
    http.end();

  } else {
    Serial.println("Gagal mengirim data ke server: WiFi tidak terhubung");  
  }
}

// =====================
// KONFIGURASI WIFI & MQTT
// =====================

#define WIFI_SSID "JTI-POLINEMA-2G"
#define WIFI_PASSWORD "jtifast!"

WiFiClient espClient;

// =====================
// SETUP KONEKSI WIFI & MQTT
// =====================

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retry = 0;

  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Gagal terhubung ke WiFi");
    return;
  } else {
  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());
  }
}

// void reconnect() {
//   while (!client.connected()) {
//     Serial.print("Attempting MQTT connection...");
//     if (client.connect("ESP32_TABLE_12")) {
//       Serial.println("connected");
//     } else {
//       Serial.print("failed, rc=");
//       Serial.print(client.state());
//       Serial.println(" try again in 2 seconds");
//       delay(2000);
//     }
//   }
// }

// =====================
// KONFIGURASI MEJA
// =====================
#define TABLE_ID 12

// Kalau jarak kurang dari ini, dianggap ada orang
#define OCCUPIED_DISTANCE_CM 20

// Timeout auto check-out jika meja kosong terus
// Untuk demo: 15 detik
// Kalau final, bisa ubah ke 15 menit: 15 * 60 * 1000
#define AUTO_CHECKOUT_TIMEOUT 15000

// =====================
// PIN RFID RC522
// =====================
#define SS_PIN   5
#define RST_PIN  22

// =====================
// PIN ULTRASONIK HC-SR04
// =====================
#define TRIG_PIN 26
#define ECHO_PIN 27

MFRC522 rfid(SS_PIN, RST_PIN);

// UID user yang sedang check-in
String currentUID = "";

// Status meja
bool isCheckedIn = false;

// Untuk auto check-out
unsigned long emptyStartTime = 0;
bool emptyTimerStarted = false;

// Cooldown RFID supaya tidak kebaca berkali-kali saat kartu masih ditempel
unsigned long lastRFIDReadTime = 0;
const unsigned long rfidCooldown = 2000;

// Timer monitoring
unsigned long lastMonitorTime = 0;
const unsigned long monitorInterval = 2000;

// // =====================
// // FUNGSI PUBLISH MQTT
// // =====================

// void publishToMQTT(JsonDocument& doc) {
//   if (!client.connected()) {
//     reconnect();
//   }

//   client.loop();

//   String payload;
//   serializeJson(doc, payload);
//   bool success = client.publish("cafe/table12/events", payload.c_str());

//   if (success) {
//     Serial.println("MQTT publish successful");
//   } else {
//     Serial.println("MQTT publish failed");
//   }

//   Serial.println(payload);
//   Serial.println("-----------------------");
// }

// =====================
// FUNGSI AMBIL UID RFID
// =====================
String getUID() {
  String uidString = "";

  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) {
      uidString += "0";
    }

    uidString += String(rfid.uid.uidByte[i], HEX);

    if (i < rfid.uid.size - 1) { 
      uidString += ":";
    }
  }

  uidString.toUpperCase();
  return uidString;
}

// =====================
// FUNGSI BACA JARAK ULTRASONIK
// =====================
float bacaJarakCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long durasi = pulseIn(ECHO_PIN, HIGH, 30000);

  if (durasi == 0) {
    return -1;
  }

  float jarak = durasi * 0.0343 / 2;
  return jarak;
}

// =====================
// FUNGSI CEK OCCUPIED
// =====================
bool cekOccupied(float distance) {
  if (distance > 0 && distance < OCCUPIED_DISTANCE_CM) {
    return true;
  }

  return false;
}

// =====================
// PRINT JSON EVENT
// =====================
void printEventJson(
  String eventType,
  String uid,
  float distance,
  String reason = "",
  String activeUID = ""
) {
  bool isOccupied = cekOccupied(distance);

  StaticJsonDocument<384> doc;

  doc["tableId"] = TABLE_ID;
  doc["uid"] = uid;
  doc["event"] = eventType;
  doc["isCheckedIn"] = isCheckedIn;
  doc["isOccupied"] = isOccupied;

  if (activeUID != "") {
    doc["currentUID"] = activeUID;
  }

  if (reason != "") {
    doc["reason"] = reason;
  }

  if (distance < 0) {
    doc["distance"] = nullptr;
  } else {
    doc["distance"] = distance;
  }

  doc["timestamp"] = millis();

  serializeJsonPretty(doc, Serial);
  Serial.println();
  Serial.println("-----------------------");

  sendToServer(doc);
  // publishToMQTT(doc);
}

// =====================
// PRINT JSON MONITORING
// =====================
void printMonitoringJson(float distance) {
  bool isOccupied = cekOccupied(distance);

  StaticJsonDocument<384> doc;

  doc["tableId"] = TABLE_ID;
  doc["uid"] = currentUID;
  doc["event"] = "MONITORING";
  doc["isCheckedIn"] = isCheckedIn;
  doc["isOccupied"] = isOccupied;

  if (distance < 0) {
    doc["distance"] = nullptr;
  } else {
    doc["distance"] = distance;
  }

  doc["timestamp"] = millis();

  serializeJsonPretty(doc, Serial);
  Serial.println();
  Serial.println("-----------------------");

  sendToServer(doc);
  // publishToMQTT(doc);
}

// =====================
// AUTO CHECK OUT
// =====================
void handleAutoCheckout(float distance) {
  bool isOccupied = cekOccupied(distance);

  static unsigned long lastOccupiedTime = 0;
  static bool autoCheckoutWarningPrinted = false;

  // Kalau belum ada yang check-in, reset data
  if (!isCheckedIn) {
    lastOccupiedTime = millis();
    autoCheckoutWarningPrinted = false;
    return;
  }

  // Kalau meja masih terdeteksi ada orang
  if (isOccupied) {
    lastOccupiedTime = millis();
    autoCheckoutWarningPrinted = false;
    return;
  }

  // Kalau sudah check-in tapi sensor membaca kosong
  unsigned long emptyDuration = millis() - lastOccupiedTime;

  // Print peringatan sekali saja
  if (!autoCheckoutWarningPrinted) {
    Serial.println("Meja kosong terdeteksi, menunggu AUTO_CHECK_OUT...");
    autoCheckoutWarningPrinted = true;
  }

  // Kalau kosong terus sampai timeout
  if (emptyDuration >= AUTO_CHECKOUT_TIMEOUT) {
    String oldUID = currentUID;

    isCheckedIn = false;
    currentUID = "";

    autoCheckoutWarningPrinted = false;
    lastOccupiedTime = millis();

    Serial.println("AUTO CHECK OUT:");
    printEventJson("AUTO_CHECK_OUT", oldUID, distance, "EMPTY_TIMEOUT");
  }
}

// =====================
// HANDLE TAP RFID
// =====================
void handleRFIDTap(String tappedUID, float distance) {
  bool isOccupied = cekOccupied(distance);

  // Kondisi 1:
  // Belum ada yang check-in
  if (!isCheckedIn) {
    if (isOccupied) {
      // CHECK IN berhasil
      currentUID = tappedUID;
      isCheckedIn = true;

      emptyTimerStarted = false;
      emptyStartTime = 0;

      Serial.println("CHECK IN:");
      printEventJson("CHECK_IN", currentUID, distance);
    } else {
      // CHECK IN ditolak karena tidak ada orang
      Serial.println("CHECK IN DITOLAK:");
      printEventJson("CHECK_IN_REJECTED", tappedUID, distance, "NOT_OCCUPIED");
    }

    return;
  }

  // Kondisi 2:
  // Sudah ada yang check-in, kartu yang sama tap
  if (isCheckedIn && tappedUID == currentUID) {
    String oldUID = currentUID;

    isCheckedIn = false;
    currentUID = "";

    emptyTimerStarted = false;
    emptyStartTime = 0;

    Serial.println("CHECK OUT:");
    printEventJson("CHECK_OUT", oldUID, distance);

    return;
  }

  // Kondisi 3:
  // Sudah ada yang check-in, tapi kartu berbeda tap
  if (isCheckedIn && tappedUID != currentUID) {
    Serial.println("AKSES DITOLAK:");
    printEventJson(
      "CHECK_IN_REJECTED",
      tappedUID,
      distance,
      "TABLE_ALREADY_USED_BY_OTHER_UID",
      currentUID
    );

    return;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  setup_wifi();
  // client.setServer(MQTT_BROKER, 1883);

  SPI.begin();
  rfid.PCD_Init();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

void loop() {
  // if (!client.connected()) {
  //   reconnect();
  // } 
  // client.loop();

  float distance = bacaJarakCM();

  // Cek auto check-out
  handleAutoCheckout(distance);

  // Cek RFID
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    unsigned long now = millis();

    if (now - lastRFIDReadTime > rfidCooldown) {
      String tappedUID = getUID();

      handleRFIDTap(tappedUID, distance);

      lastRFIDReadTime = now;
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }

  // Monitoring setiap 2 detik
  if (millis() - lastMonitorTime >= monitorInterval) {
    printMonitoringJson(distance);
    lastMonitorTime = millis();
  }
}