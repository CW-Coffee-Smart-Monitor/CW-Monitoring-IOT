#include "NetworkManager.h"

NetworkManager::NetworkManager(const char* ssid, const char* password,
                               const char* socketHost, uint16_t socketPort,
                               const char* apiBaseUrl, const char* webhookUrl)
    : ssid_(ssid), password_(password),
      socketHost_(socketHost), socketPort_(socketPort),
      apiBaseUrl_(apiBaseUrl), webhookUrl_(webhookUrl) {}

void NetworkManager::setupWiFi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid_);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid_, password_);

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

bool NetworkManager::isWiFiConnected() {
    return (WiFi.status() == WL_CONNECTED);
}

void NetworkManager::connectSocket() {
    if (!isWiFiConnected()) {
        Serial.println("Tidak bisa connect socket: WiFi belum terhubung");
        return;
    }

    Serial.print("[Socket] Menghubungkan ke ");
    Serial.print(socketHost_);
    Serial.print(":");
    Serial.println(socketPort_);

    while (!socketClient_.connected()) {
        if (socketClient_.connect(socketHost_, socketPort_)) {
            Serial.println("[Socket] Terhubung ke server!");
        } else {
            Serial.println("[Socket] Gagal connect, coba lagi 2 detik...");
            delay(2000);
        }
    }
}

bool NetworkManager::isSocketConnected() {
    return socketClient_.connected();
}

void NetworkManager::maintainSocket() {
    if (!isWiFiConnected()) {
        return;
    }

    if (!socketClient_.connected()) {
        Serial.println("[WARN] Socket terputus, reconnect...");
        socketClient_.stop();
        connectSocket();
    }
}

bool NetworkManager::readSocketCommand(String& outCommand) {
    if (socketClient_.connected() && socketClient_.available()) {
        outCommand = socketClient_.readStringUntil('\n');
        return true;
    }
    return false;
}

void NetworkManager::sendSocketJson(const JsonDocument& doc) {
    if (!isWiFiConnected()) {
        Serial.println("Gagal mengirim data ke socket server: WiFi tidak terhubung");
        return;
    }

    maintainSocket();

    String payload;
    serializeJson(doc, payload);
    socketClient_.println(payload);

    Serial.print("Socket data sent: ");
    Serial.println(payload);
    Serial.println("-----------------------");
}

void NetworkManager::sendWebhookJson(const JsonDocument& doc) {
    if (!isWiFiConnected()) {
        Serial.println("Gagal mengirim data ke server: WiFi tidak terhubung");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, webhookUrl_);
    http.addHeader("Content-Type", "application/json");

    String jsonString;
    serializeJson(doc, jsonString);

    int responseCode = http.POST(jsonString);
    Serial.print("HTTP Response: ");
    Serial.println(responseCode);
    http.end();
}

bool NetworkManager::validateRFID(int tableId, const String& uid) {
    if (!isWiFiConnected()) {
        return false;
    }

    HTTPClient http;
    String url = String(apiBaseUrl_) + "/api/rfid/validate/" + String(tableId) + "/" + uid;

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

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.print("[RFID VALIDATE] JSON ERROR: ");
        Serial.println(err.c_str());
        http.end();
        return false;
    }

    bool valid = doc["valid"] | false;
    Serial.print("[RFID VALIDATE] RESULT = ");
    Serial.println(valid);

    http.end();
    return valid;
}

TableStatusResponse NetworkManager::fetchTableStatus(int tableId) {
    TableStatusResponse response = {false, false, false, "", ""};

    if (!isWiFiConnected()) {
        Serial.println("[DEBUG] WiFi tidak terhubung");
        return response;
    }

    HTTPClient http;
    String url = String(apiBaseUrl_) + "/api/status/" + String(tableId);

    Serial.print("[DEBUG] URL = ");
    Serial.println(url);

    http.begin(url);
    int code = http.GET();

    Serial.print("[DEBUG] HTTP Code = ");
    Serial.println(code);

    if (code != 200) {
        if (code < 0) {
            Serial.print("[DEBUG] Error = ");
            Serial.println(http.errorToString(code));
        }
        Serial.println("[DEBUG] Request gagal");
        http.end();
        return response;
    }

    String payload = http.getString();
    Serial.println("[DEBUG] Payload API:");
    Serial.println(payload);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.print("[DEBUG] JSON Parse Error: ");
        Serial.println(err.c_str());
        http.end();
        return response;
    }

    response.success = true;
    response.isCheckedIn = doc["isCheckedIn"] | false;
    response.isReserved = !doc["reservation"].isNull();

    if (response.isReserved) {
        if (!doc["reservation"]["status"].isNull()) {
            response.reservationStatus = doc["reservation"]["status"].as<const char*>();
        }
        if (!doc["reservation"]["tableName"].isNull()) {
            response.tableName = doc["reservation"]["tableName"].as<const char*>();
        }
    }

    http.end();
    return response;
}

void NetworkManager::sendEvent(int tableId, const String& eventType, const String& uid,
                               bool isCheckedIn, bool isReserved, bool isOccupied,
                               float distance, const String& reason, const String& activeUID) {
    JsonDocument doc;

    doc["tableId"] = tableId;
    doc["uid"] = uid;
    doc["event"] = eventType;
    doc["isCheckedIn"] = isCheckedIn;
    doc["isReserved"] = isReserved;
    doc["isOccupied"] = isOccupied;

    if (activeUID.length() > 0) {
        doc["currentUID"] = activeUID;
    }

    if (reason.length() > 0) {
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

    sendSocketJson(doc);
}

void NetworkManager::sendMonitoring(int tableId, const String& uid,
                                    bool isCheckedIn, bool isReserved, bool isOccupied,
                                    float distance) {
    JsonDocument doc;

    doc["tableId"] = tableId;
    doc["uid"] = uid;
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

    sendSocketJson(doc);
}
