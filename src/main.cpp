#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <TableLogic.h>
#include <CommandHandler.h>

// =====================
// KIRIM WEBHOOK
// =====================
void sendToServer(JsonDocument& doc) {
  if (WiFi.status() == WL_CONNECTED) {

    WiFiClientSecure client;
    client.setInsecure(); // Disable certificate validation (not recommended for production)

    HTTPClient http;
    http.begin(client, "https://fasilita.my.id/api/webhook");
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
#define WIFI_SSID "WAWAN"
#define WIFI_PASSWORD "wawannnn"

#define SOCKET_HOST "152.42.207.49"
#define SOCKET_PORT 9001

#define API_BASE_URL "http://152.42.207.49:9002"

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
  WiFiClient test;

Serial.println("=== TEST TCP ===");

if (test.connect("152.42.207.49", 9002)) {
  Serial.println("TCP 9002 OK");
  test.stop();
} else {
  Serial.println("TCP 9002 FAILED");
}

if (test.connect("152.42.207.49", 9001)) {
  Serial.println("TCP 9001 OK");
  test.stop();
} else {
  Serial.println("TCP 9001 FAILED");
}

  Serial.println("");
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

TableManager tableManager(TABLE_ID, OCCUPIED_DISTANCE_CM, AUTO_CHECKOUT_TIMEOUT);

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
bool isReserved = false;


// Cooldown RFID supaya tidak kebaca berkali-kali saat kartu masih ditempel
unsigned long lastRFIDReadTime = 0;
const unsigned long rfidCooldown = 2000;

// Timer monitoring
unsigned long lastMonitorTime = 0;
const unsigned long monitorInterval = 2000;

unsigned long lastApiCheck = 0;
const unsigned long apiInterval = 5000;

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

  CommandResult res = CommandHandler::parse(command.c_str());

  if (res.modifiesLedOverride) {
    manualLedOverride = res.manualLedOverride;
  }

  if (res.modifiesReservation) {
    isReserved = res.isReserved;
    tableManager.setReserved(res.isReserved);
  }

  switch (res.type) {
    case CommandType::LED_GREEN:
      ledGreen();
      Serial.println("[AKSI] LED GREEN / ON");
      break;
    case CommandType::LED_RED:
      ledRed();
      Serial.println("[AKSI] LED RED");
      break;
    case CommandType::LED_BLUE:
      ledBlue();
      Serial.println("[AKSI] LED BLUE");
      break;
    case CommandType::LED_YELLOW:
      ledYellow();
      Serial.println("[AKSI] LED YELLOW");
      break;
    case CommandType::LED_CYAN:
      ledCyan();
      Serial.println("[AKSI] LED CYAN");
      break;
    case CommandType::LED_PURPLE:
      ledPurple();
      Serial.println("[AKSI] LED PURPLE");
      break;
    case CommandType::LED_OFF:
      Serial.println("[AKSI] Kembali ke mode indikator otomatis");
      break;
    case CommandType::MODE_AUTO:
      Serial.println("[AKSI] Mode LED otomatis aktif");
      break;
    case CommandType::RESERVE:
      Serial.println("[AKSI] Meja direservasi");
      break;
    case CommandType::CANCEL_RESERVATION:
      Serial.println("[AKSI] Reservasi dibatalkan");
      break;
    default:
      Serial.println("[WARN] Perintah tidak dikenal");
      break;
  }
}

void readSocketCommand() {
  while (socketClient.connected() && socketClient.available()) {
    String command = socketClient.readStringUntil('\n');
    handleSocketCommand(command);
  }
}

void fetchTableStatus() {

  Serial.println();
  Serial.println("[DEBUG] fetchTableStatus dipanggil");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[DEBUG] WiFi tidak terhubung");
    return;
  }

  HTTPClient http;

  String url =
      String(API_BASE_URL)
      + "/api/status/"
      + String(TABLE_ID);

  Serial.print("[DEBUG] URL = ");
  Serial.println(url);

  http.begin(url);

  int code = http.GET();

  Serial.print("[DEBUG] HTTP Code = ");
  Serial.println(code);

  if (code < 0) {
    Serial.print("[DEBUG] Error = ");
    Serial.println(http.errorToString(code));
  }

  if (code != 200) {
    Serial.println("[DEBUG] Request gagal");
    http.end();
    return;
  }

  String payload = http.getString();

  Serial.println("[DEBUG] Payload API:");
  Serial.println(payload);

  StaticJsonDocument<2048> doc;

  DeserializationError err =
      deserializeJson(doc, payload);

  if (err) {
    Serial.print("[DEBUG] JSON Parse Error: ");
    Serial.println(err.c_str());
    http.end();
    return;
  }

  isCheckedIn =
      doc["isCheckedIn"] | false;

  bool reservationExists =
      !doc["reservation"].isNull();

  isReserved = reservationExists;

  tableManager.setCheckedIn(isCheckedIn);
  tableManager.setReserved(isReserved);

  Serial.println();
  Serial.println("===== STATUS API =====");

  Serial.print("isCheckedIn: ");
  Serial.println(isCheckedIn);

  Serial.print("isReserved: ");
  Serial.println(isReserved);

  if (reservationExists) {

    Serial.println("Reservation ditemukan");

    if (!doc["reservation"]["status"].isNull()) {
      Serial.print("Status Reservation: ");
      Serial.println(
          doc["reservation"]["status"]
          .as<const char*>()
      );
    }

    if (!doc["reservation"]["tableName"].isNull()) {
      Serial.print("Table: ");
      Serial.println(
          doc["reservation"]["tableName"]
          .as<const char*>()
      );
    }

  } else {

    Serial.println("Tidak ada reservation");

  }

  Serial.println("======================");

  http.end();
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
  return tableManager.isOccupied(distance);
}

// =====================
// UPDATE LED BERDASARKAN STATUS MEJA
// =====================
void updateStatusLED(float distance) {

  bool isOccupied = cekOccupied(distance);

  if (isCheckedIn) {

    // sedang digunakan
    ledGreen();

  } else if (isReserved) {

    // meja direservasi
    ledCyan();

  } else if (isOccupied) {

    // ada orang tapi belum checkin
    ledYellow();

  } else {

    // kosong
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
  doc["isReserved"] = isReserved;
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
  doc["isReserved"] = isReserved;
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
  std::string checkedOutUID;
  AutoCheckoutResult result = tableManager.updateAutoCheckout(distance, millis(), checkedOutUID);

  if (result == AutoCheckoutResult::WARNING_TRIGGERED) {
    Serial.println("Meja kosong terdeteksi, menunggu AUTO_CHECK_OUT...");
  } else if (result == AutoCheckoutResult::TIMEOUT_TRIGGERED) {
    isCheckedIn = false;
    currentUID = "";

    Serial.println("AUTO CHECK OUT:");
    printEventJson("AUTO_CHECK_OUT", String(checkedOutUID.c_str()), distance, "EMPTY_TIMEOUT");

    blinkLED(ledPurple, 3, 150);
  }
}

bool validateRFID(String uid)
{
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    HTTPClient http;

    String url =
        String(API_BASE_URL) +
        "/api/rfid/validate/" +
        String(TABLE_ID) +
        "/" +
        uid;

    Serial.print("[RFID VALIDATE] ");
    Serial.println(url);

    http.begin(url);

    int code = http.GET();

    if (code != 200) {

        Serial.print("[RFID VALIDATE] HTTP ERROR: ");
        Serial.println(code);

        http.end();
        return false;
    }

    String payload = http.getString();

    Serial.print("[RFID VALIDATE] RESPONSE = ");
    Serial.println(payload);

    StaticJsonDocument<256> doc;

    DeserializationError err =
        deserializeJson(doc, payload);

    if (err) {

        Serial.print("[RFID VALIDATE] JSON ERROR: ");
        Serial.println(err.c_str());

        http.end();
        return false;
    }

    bool valid =
        doc["valid"] | false;

    Serial.print("[RFID VALIDATE] RESULT = ");
    Serial.println(valid);

    http.end();

    return valid;
}

// =====================
// HANDLE TAP RFID
// =====================
void handleRFIDTap(String tappedUID, float distance) {
  bool isAllowed = validateRFID(tappedUID);

  TapResult res = tableManager.handleRFIDTap(tappedUID.c_str(), distance, isAllowed);

  switch (res.type) {
    case TapResultType::REJECTED_UID_NOT_ALLOWED:
      Serial.println("AKSES DITOLAK");
      printEventJson(
        "CHECK_IN_REJECTED",
        tappedUID,
        distance,
        "UID_NOT_ALLOWED"
      );
      blinkLED(ledRed, 3, 150);
      break;

    case TapResultType::CHECK_IN_SUCCESS:
      currentUID = tappedUID;
      isCheckedIn = true;
      isReserved = false;

      Serial.println("CHECK IN:");
      printEventJson("CHECK_IN", currentUID, distance);
      blinkLED(ledGreen, 2, 150);
      break;

    case TapResultType::CHECK_IN_REJECTED_NOT_OCCUPIED:
      Serial.println("CHECK IN DITOLAK:");
      printEventJson(
        "CHECK_IN_REJECTED",
        tappedUID,
        distance,
        "NOT_OCCUPIED"
      );
      blinkLED(ledRed, 3, 150);
      break;

    case TapResultType::CHECK_OUT_SUCCESS: {
      String oldUID = String(res.uid.c_str());
      isCheckedIn = false;
      currentUID = "";

      Serial.println("CHECK OUT:");
      printEventJson("CHECK_OUT", oldUID, distance);
      blinkLED(ledBlue, 2, 150);
      break;
    }

    case TapResultType::REJECTED_ALREADY_USED_BY_OTHER:
      Serial.println("AKSES DITOLAK:");
      printEventJson(
        "CHECK_IN_REJECTED",
        tappedUID,
        distance,
        "TABLE_ALREADY_USED_BY_OTHER_UID",
        currentUID
      );
      blinkLED(ledRed, 3, 150);
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  setup_wifi();

  if (WiFi.status() == WL_CONNECTED) {
    connectSocketServer();
  }

  // =====================
  // TEST HTTP API
  // =====================
  HTTPClient http;

  Serial.println("=== TEST API ===");

  bool ok = http.begin("http://152.42.207.49:9002/api/status/12");

  Serial.print("BEGIN=");
  Serial.println(ok);

  if (ok) {
    int code = http.GET();

    Serial.print("CODE=");
    Serial.println(code);

    if (code > 0) {
      Serial.println(http.getString());
    } else {
      Serial.print("ERROR=");
      Serial.println(http.errorToString(code));
    }

    http.end();
  }

  Serial.println("=== END TEST API ===");

  // =====================
  // LANJUT SETUP NORMAL
  // =====================
  SPI.begin();
  rfid.PCD_Init();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(LED_R_PIN, OUTPUT);
  pinMode(LED_G_PIN, OUTPUT);
  pinMode(LED_B_PIN, OUTPUT);

  ledOff();

  Serial.println("Sistem siap.");
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

  if (millis() - lastApiCheck >= apiInterval) {
    fetchTableStatus();
    lastApiCheck = millis();
  }

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