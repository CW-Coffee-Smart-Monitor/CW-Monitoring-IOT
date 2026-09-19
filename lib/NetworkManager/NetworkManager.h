#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

struct TableStatusResponse {
    bool success;
    bool isCheckedIn;
    bool isReserved;
    String reservationStatus;
    String tableName;
};

class NetworkManager {
public:
    NetworkManager(const char* ssid, const char* password,
                   const char* socketHost, uint16_t socketPort,
                   const char* apiBaseUrl, const char* webhookUrl);

    void setupWiFi();
    bool isWiFiConnected();

    void connectSocket();
    bool isSocketConnected();
    void maintainSocket();

    bool readSocketCommand(String& outCommand);
    void sendSocketJson(const JsonDocument& doc);
    void sendWebhookJson(const JsonDocument& doc);

    bool validateRFID(int tableId, const String& uid);
    TableStatusResponse fetchTableStatus(int tableId);

    void sendEvent(int tableId, const String& eventType, const String& uid,
                   bool isCheckedIn, bool isReserved, bool isOccupied,
                   float distance, const String& reason = "", const String& activeUID = "");

    void sendMonitoring(int tableId, const String& uid,
                        bool isCheckedIn, bool isReserved, bool isOccupied,
                        float distance);

private:
    const char* ssid_;
    const char* password_;
    const char* socketHost_;
    uint16_t socketPort_;
    const char* apiBaseUrl_;
    const char* webhookUrl_;

    WiFiClient socketClient_;
};

#endif // NETWORK_MANAGER_H
