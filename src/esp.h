#pragma once

#include "common.h"
#include "settings.h"
#include <Arduino.h>
#ifdef ESP_32
#include <ESP_WiFiManager.h>
#elif defined(ESP_8266) 
#include <WiFiManager.h>
#endif


class Esp
{
#pragma region Private

private:
#pragma endregion Private

#pragma region Public

public:
    // constructor
    Esp(void) { _alive = false; }

    // destructor
    ~Esp(void) { _alive = false; }

#pragma endregion Public
    void begin();
    void loop();
    void reset();
    void wiFiSetup();
    void wiFiReconnect();
    void setupOta();

    String getMacHex(void);

    const char *getWiFiConfigPass(void) { return _wifiConfigPass; }

    const char *getWiFiConfigAP(void) { return _wifiConfigAP; }

    // Original source: https://arduino.stackexchange.com/a/1237
    String getSubtringField(String data, char separator, int index);

    ////////////////////////////////////////////////////////////////////////////////
    String printHex8(uint8_t *data, uint8_t length);

#pragma region Protected

protected:
    bool _alive;
    const char *_wifiConfigPass = WIFI_CONFIG_PASSWORD;          // First-time config WPA2 password
    const char *_wifiConfigAP = WIFI_CONFIG_AP;                  // First-time config SSID
    const uint32_t _connectTimeout = CONNECTION_TIMEOUT;         // Timeout for WiFi and MQTT connection attempts in seconds
    const uint32_t _reConnectTimeout = RECONNECT_TIMEOUT;        // Timeout for WiFi reconnection attempts in seconds
    uint8_t _espMac[6];                                          // Byte array to store our MAC address

#pragma endregion Protected
};