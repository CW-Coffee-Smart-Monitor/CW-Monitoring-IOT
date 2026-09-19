#include <Arduino.h>
#include <MFRC522.h>
#include <SPI.h>

#include "CommandHandler.h"
#include "Config.h"
#include "HardwareDrivers.h"
#include "NetworkManager.h"
#include "TableLogic.h"

// ==========================================
// HARDWARE & NETWORK MODULE INSTANCES
// ==========================================
LedIndicator led(LED_R_PIN, LED_G_PIN, LED_B_PIN);
UltrasonicSensor ultrasonic(TRIG_PIN, ECHO_PIN);
MFRC522 rfid(SS_PIN, RST_PIN);
NetworkManager net(WIFI_SSID, WIFI_PASSWORD, SOCKET_HOST, SOCKET_PORT, API_BASE_URL, WEBHOOK_URL);
TableManager tableManager(TABLE_ID, OCCUPIED_DISTANCE_CM, AUTO_CHECKOUT_TIMEOUT);

// ==========================================
// STATE VARIABLES & TIMERS
// ==========================================
bool manualLedOverride = false;
unsigned long lastRFIDReadTime = 0;
unsigned long lastMonitorTime = 0;
unsigned long lastApiCheck = 0;

// ==========================================
// RFID UID HELPER
// ==========================================
String readRFIDUID() {
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

// ==========================================
// DISPATCH SOCKET COMMANDS
// ==========================================
void handleIncomingCommand(const String& command) {
    Serial.print("[TERIMA] Perintah dari server: ");
    Serial.println(command);

    CommandResult res = CommandHandler::parse(command.c_str());

    if (res.modifiesLedOverride) {
        manualLedOverride = res.manualLedOverride;
    }

    if (res.modifiesReservation) {
        tableManager.setReserved(res.isReserved);
    }

    switch (res.type) {
        case CommandType::LED_GREEN:
            led.green();
            Serial.println("[AKSI] LED GREEN / ON");
            break;
        case CommandType::LED_RED:
            led.red();
            Serial.println("[AKSI] LED RED");
            break;
        case CommandType::LED_BLUE:
            led.blue();
            Serial.println("[AKSI] LED BLUE");
            break;
        case CommandType::LED_YELLOW:
            led.yellow();
            Serial.println("[AKSI] LED YELLOW");
            break;
        case CommandType::LED_CYAN:
            led.cyan();
            Serial.println("[AKSI] LED CYAN");
            break;
        case CommandType::LED_PURPLE:
            led.purple();
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

// ==========================================
// HANDLE RFID TAP EVENT
// ==========================================
void handleRFIDTapEvent(const String& tappedUID, float distance) {
    bool isAllowed = net.validateRFID(tableManager.getTableId(), tappedUID);
    TapResult res = tableManager.handleRFIDTap(tappedUID.c_str(), distance, isAllowed);

    switch (res.type) {
        case TapResultType::REJECTED_UID_NOT_ALLOWED:
            Serial.println("AKSES DITOLAK");
            net.sendEvent(tableManager.getTableId(), "CHECK_IN_REJECTED", tappedUID, tableManager.isCheckedIn(),
                          tableManager.isReserved(), tableManager.isOccupied(distance), distance, "UID_NOT_ALLOWED");
            led.blink(&LedIndicator::red, 3, 150);
            break;

        case TapResultType::CHECK_IN_SUCCESS:
            Serial.println("CHECK IN:");
            net.sendEvent(tableManager.getTableId(), "CHECK_IN", String(res.uid.c_str()), tableManager.isCheckedIn(),
                          tableManager.isReserved(), tableManager.isOccupied(distance), distance);
            led.blink(&LedIndicator::green, 2, 150);
            break;

        case TapResultType::CHECK_IN_REJECTED_NOT_OCCUPIED:
            Serial.println("CHECK IN DITOLAK:");
            net.sendEvent(tableManager.getTableId(), "CHECK_IN_REJECTED", tappedUID, tableManager.isCheckedIn(),
                          tableManager.isReserved(), tableManager.isOccupied(distance), distance, "NOT_OCCUPIED");
            led.blink(&LedIndicator::red, 3, 150);
            break;

        case TapResultType::CHECK_OUT_SUCCESS:
            Serial.println("CHECK OUT:");
            net.sendEvent(tableManager.getTableId(), "CHECK_OUT", String(res.uid.c_str()), tableManager.isCheckedIn(),
                          tableManager.isReserved(), tableManager.isOccupied(distance), distance);
            led.blink(&LedIndicator::blue, 2, 150);
            break;

        case TapResultType::REJECTED_ALREADY_USED_BY_OTHER:
            Serial.println("AKSES DITOLAK:");
            net.sendEvent(tableManager.getTableId(), "CHECK_IN_REJECTED", tappedUID, tableManager.isCheckedIn(),
                          tableManager.isReserved(), tableManager.isOccupied(distance), distance,
                          "TABLE_ALREADY_USED_BY_OTHER_UID", String(res.activeUID.c_str()));
            led.blink(&LedIndicator::red, 3, 150);
            break;
    }
}

// ==========================================
// HANDLE AUTO CHECK-OUT TIMEOUT
// ==========================================
void handleAutoCheckoutEvent(float distance) {
    std::string checkedOutUID;
    AutoCheckoutResult result = tableManager.updateAutoCheckout(distance, millis(), checkedOutUID);

    if (result == AutoCheckoutResult::WARNING_TRIGGERED) {
        Serial.println("Meja kosong terdeteksi, menunggu AUTO_CHECK_OUT...");
    } else if (result == AutoCheckoutResult::TIMEOUT_TRIGGERED) {
        Serial.println("AUTO CHECK OUT:");
        net.sendEvent(tableManager.getTableId(), "AUTO_CHECK_OUT", String(checkedOutUID.c_str()),
                      tableManager.isCheckedIn(), tableManager.isReserved(), tableManager.isOccupied(distance),
                      distance, "EMPTY_TIMEOUT");
        led.blink(&LedIndicator::purple, 3, 150);
    }
}

// ==========================================
// ARDUINO SETUP
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    led.begin();
    ultrasonic.begin();

    SPI.begin();
    rfid.PCD_Init();

    net.setupWiFi();
    if (net.isWiFiConnected()) {
        net.connectSocket();
    }

    Serial.println("Sistem siap.");
}

// ==========================================
// ARDUINO MAIN LOOP
// ==========================================
void loop() {
    if (!net.isWiFiConnected()) {
        Serial.println("WiFi belum terhubung, skip pengiriman data...");
        delay(2000);
        return;
    }

    net.maintainSocket();

    // 1. Baca perintah dari socket server
    String incomingCmd;
    while (net.readSocketCommand(incomingCmd)) {
        handleIncomingCommand(incomingCmd);
    }

    // 2. Baca jarak ultrasonik
    float distance = ultrasonic.readDistanceCM();

    // 3. Cek status API secara berkala
    if (millis() - lastApiCheck >= API_INTERVAL_MS) {
        TableStatusResponse status = net.fetchTableStatus(tableManager.getTableId());
        if (status.success) {
            tableManager.setCheckedIn(status.isCheckedIn);
            tableManager.setReserved(status.isReserved);
        }
        lastApiCheck = millis();
    }

    // 4. Evaluasi auto checkout
    handleAutoCheckoutEvent(distance);

    // 5. Cek pembacaan RFID
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
        unsigned long now = millis();
        if (now - lastRFIDReadTime > RFID_COOLDOWN_MS) {
            String tappedUID = readRFIDUID();
            handleRFIDTapEvent(tappedUID, distance);
            lastRFIDReadTime = now;
        }
        rfid.PICC_HaltA();
        rfid.PCD_StopCrypto1();
    }

    // 6. Kirim data monitoring berkala
    if (millis() - lastMonitorTime >= MONITOR_INTERVAL_MS) {
        net.sendMonitoring(tableManager.getTableId(), String(tableManager.getCurrentUID().c_str()),
                           tableManager.isCheckedIn(), tableManager.isReserved(), tableManager.isOccupied(distance),
                           distance);
        lastMonitorTime = millis();
    }

    // 7. Update status LED (jika tidak sedang di-override manual)
    if (!manualLedOverride) {
        led.updateStatus(tableManager.isCheckedIn(), tableManager.isReserved(), tableManager.isOccupied(distance));
    }
}