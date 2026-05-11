#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ArduinoJson.h>
#include <WiFi.h>
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
// KONFIGURASI WIFI & SOCKET TCP
// =====================
#define WIFI_SSID "Xiaomi 15T"
#define WIFI_PASSWORD "1sampai7"

#define SOCKET_HOST "152.42.207.49"
#define SOCKET_PORT 9001

WiFiClient socketClient;

// =====================
// SETUP KONEKSI WIFI
// =====================
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retry = 0;

  while (WiFi.status() != WL_CONNECTED && retry < 40) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println();
    Serial.println("Gagal terhubung ke WiFi");
    Serial.print("WiFi status: ");
    Serial.println(WiFi.status());
    return;
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// =====================
// TEST KONEKSI TCP SOCKET
// =====================
void testTcpConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Tidak bisa test TCP: WiFi belum terhubung");
    return;
  }

  Serial.print("Testing TCP to socket server ");
  Serial.print(SOCKET_HOST);
  Serial.print(":");
  Serial.println(SOCKET_PORT);

  WiFiClient testClient;

  if (testClient.connect(SOCKET_HOST, SOCKET_PORT)) {
    Serial.println("TCP connection to socket server successful");
    testClient.stop();
  } else {
    Serial.println("TCP connection to socket server failed");
  }
}

// =====================
// KONEKSI KE SOCKET SERVER
// Praktikum 10: koneksi dibuat tetap terbuka
// =====================
void connectSocketServer() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Tidak bisa connect socket: WiFi belum terhubung");
    return;
  }

  Serial.print("[Socket] Menghubungkan ke ");
  Serial.print(SOCKET_HOST);
  Serial.print(":");
  Serial.println(SOCKET_PORT);

  while (!socketClient.connected()) {
    if (socketClient.connect(SOCKET_HOST, SOCKET_PORT)) {
      Serial.println("[Socket] Terhubung ke server!");
    } else {
      Serial.println("[Socket] Gagal connect, coba lagi 2 detik...");
      delay(2000);
    }
  }
}

// =====================
// FUNGSI KIRIM DATA TCP SOCKET
// Praktikum 10: kirim data tanpa menutup koneksi
// =====================
void sendToSocket(JsonDocument& doc) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Gagal mengirim data ke socket server: WiFi tidak terhubung");
    return;
  }

  if (!socketClient.connected()) {
    Serial.println("[WARN] Socket terputus, reconnect...");
    socketClient.stop();
    connectSocketServer();
  }

  String payload;
  serializeJson(doc, payload);

  socketClient.println(payload);

  Serial.print("Socket data sent: ");
  Serial.println(payload);
  Serial.println("-----------------------");
}

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

// =====================
// PIN LED RGB COMMON ANODE
// R: D2  / GPIO 2
// G: D4  / GPIO 4
// B: D21 / GPIO 21
// Common / kaki panjang -> 3V3
// =====================
#define LED_R_PIN 2
#define LED_G_PIN 4
#define LED_B_PIN 21

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

// ==========================================
// FUNGSI LED RGB
// Common Anode: LOW = nyala, HIGH = mati
// ==========================================
void setRGB(bool red, bool green, bool blue) {
  digitalWrite(LED_R_PIN, red   ? LOW : HIGH);
  digitalWrite(LED_G_PIN, green ? LOW : HIGH);
  digitalWrite(LED_B_PIN, blue  ? LOW : HIGH);
}

void ledOff()    { setRGB(true,  true,  true);  }
void ledRed()    { setRGB(false, true,  true);  }
void ledGreen()  { setRGB(true,  false, true);  }
void ledBlue()   { setRGB(true,  true,  false); }
void ledYellow() { setRGB(false, false, true);  }
void ledCyan()   { setRGB(true,  false, false); }
void ledPurple() { setRGB(false, true,  false); }

void blinkLED(void (*colorFunc)(), int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    colorFunc();
    delay(delayMs);
    ledOff();
    delay(delayMs);
  }
}

// =====================
// FUNGSI TERIMA PERINTAH DARI SERVER
// =====================
bool manualLedOverride = false;

void handleSocketCommand(String command) {
  command.trim();

  if (command.length() == 0) {
    return;
  }

  Serial.print("[TERIMA] Perintah dari server: ");
  Serial.println(command);

  if (command.equalsIgnoreCase("led-on") || command.equalsIgnoreCase("led-green")) {
    manualLedOverride = true;
    ledGreen();
    Serial.println("[AKSI] LED GREEN / ON");
  } else if (command.equalsIgnoreCase("led-red")) {
    manualLedOverride = true;
    ledRed();
    Serial.println("[AKSI] LED RED");
  } else if (command.equalsIgnoreCase("led-blue")) {
    manualLedOverride = true;
    ledBlue();
    Serial.println("[AKSI] LED BLUE");
  } else if (command.equalsIgnoreCase("led-yellow")) {
    manualLedOverride = true;
    ledYellow();
    Serial.println("[AKSI] LED YELLOW");
  } else if (command.equalsIgnoreCase("led-cyan")) {
    manualLedOverride = true;
    ledCyan();
    Serial.println("[AKSI] LED CYAN");
  } else if (command.equalsIgnoreCase("led-purple")) {
    manualLedOverride = true;
    ledPurple();
    Serial.println("[AKSI] LED PURPLE");
  } else if (command.equalsIgnoreCase("led-off") || command.equalsIgnoreCase("auto")) {
    manualLedOverride = false;
    Serial.println("[AKSI] Kembali ke mode indikator otomatis");
  } else if (command.equalsIgnoreCase("auto")) {
    manualLedOverride = false;
    Serial.println("[AKSI] Mode LED otomatis aktif");
  } else {
    Serial.println("[WARN] Perintah tidak dikenal");
  }
}

void readSocketCommand() {
  while (socketClient.connected() && socketClient.available()) {
    String command = socketClient.readStringUntil('\n');
    handleSocketCommand(command);
  }
}

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
// UPDATE LED BERDASARKAN STATUS MEJA
// =====================
void updateStatusLED(float distance) {
  bool isOccupied = cekOccupied(distance);

  if (isCheckedIn) {
    // Meja sedang dipakai / sudah check-in
    ledGreen();
  } else if (isOccupied) {
    // Ada orang, tapi belum check-in
    ledYellow();
  } else {
    // Kosong
    ledBlue();
  }
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

  // sendToServer(doc);
  sendToSocket(doc);
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

  // sendToServer(doc);
  sendToSocket(doc);
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

    blinkLED(ledPurple, 3, 150);
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

      blinkLED(ledGreen, 2, 150);
    } else {
      // CHECK IN ditolak karena tidak ada orang
      Serial.println("CHECK IN DITOLAK:");
      printEventJson("CHECK_IN_REJECTED", tappedUID, distance, "NOT_OCCUPIED");

      blinkLED(ledRed, 3, 150);
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

    blinkLED(ledBlue, 2, 150);

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

    blinkLED(ledRed, 3, 150);

    return;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  setup_wifi();

  if (WiFi.status() == WL_CONNECTED) {
    connectSocketServer();
  }

  SPI.begin();
  rfid.PCD_Init();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(LED_R_PIN, OUTPUT);
  pinMode(LED_G_PIN, OUTPUT);
  pinMode(LED_B_PIN, OUTPUT);

  ledOff();

  Serial.println("Sistem meja RFID + Ultrasonik + WiFi + TCP Socket Praktikum 10 siap.");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi belum terhubung, skip pengiriman data...");
    delay(2000);
    return;
  }

  if (!socketClient.connected()) {
    Serial.println("[WARN] Koneksi ke server terputus. Mencoba reconnect...");
    socketClient.stop();
    connectSocketServer();
  }

  readSocketCommand();

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

  // Terima perintah dari server setelah kirim data
  readSocketCommand();

  // Update indikator LED
  if (!manualLedOverride) {
    updateStatusLED(distance);
  }
}