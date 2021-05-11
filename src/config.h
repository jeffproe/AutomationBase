#pragma once

#include "settings.h"
#include <Arduino.h>

class Config
{
#pragma region Private

private:
#pragma endregion Private

#pragma region Public

public:
    // constructor
    Config(void)
    {
        _alive = true;
        setWIFISSID(DEFAULT_WIFI_SSID);
        setWIFIPass(DEFAULT_WIFI_PASS);

        // These defaults may be overwritten with values saved by the web interface
        // Note that MQTT prefers dotted quad address, but MQTTS prefers fully qualified domain names (fqdn)
        // Note that MQTTS works best using NTP to obtain Time
        setMQTTServer(DEFAULT_MQTT_SERVER);
        setMQTTPort(DEFAULT_MQTT_PORT);
        setMQTTUser(DEFAULT_MQTT_USER);
        setMQTTPassword(DEFAULT_MQTT_PASS);
        setNodeName(DEFAULT_NODE_NAME);
        setGroupName(DEFAULT_GROUP_NAME);
        setMDSNEnabled(MDNS_ENABLED);

        _shouldSaveConfig = false; // Flag to save json config to SPIFFS
    }

    // destructor
    ~Config(void) { _alive = false; }

#pragma endregion Public
    void begin();

    void loop();

    void readFile();

    void saveCallback();

    void saveFileIfNeeded();

    void saveFile();

    void clearFileSystem();

    // we are going to have quite a count of getters and setters
    // can we streamline them? (basic C++ syntax says "no")

    char *getWIFISSID(void) { return _wifiSSID; }
    void setWIFISSID(const char *value)
    {
        strncpy(_wifiSSID, value, 32);
        _wifiSSID[31] = '\0';
    }

    char *getWIFIPass(void) { return _wifiPass; }
    void setWIFIPass(const char *value)
    {
        strncpy(_wifiPass, value, 64);
        _wifiPass[63] = '\0';
    }

    char *getMQTTServer(void) { return _mqttServer; }
    void setMQTTServer(const char *value)
    {
        strncpy(_mqttServer, value, 64);
        _mqttServer[63] = '\0';
    }

    char *getMQTTPort(void) { return _mqttPort; }
    void setMQTTPort(const char *value)
    {
        strncpy(_mqttPort, value, 6);
        _mqttPort[5] = '\0';
    }

    char *getMQTTUser(void) { return _mqttUser; }
    void setMQTTUser(const char *value)
    {
        strncpy(_mqttUser, value, 32);
        _mqttUser[31] = '\0';
    }

    char *getMQTTPassword(void) { return _mqttPassword; }
    void setMQTTPassword(const char *value)
    {
        strncpy(_mqttPassword, value, 32);
        _mqttPassword[31] = '\0';
    }

    char *getNodeName(void) { return _nodeName; }
    void setNodeName(const char *value)
    {
        strncpy(_nodeName, value, 16);
        _nodeName[15] = '\0';
    }

    char *getGroupName(void) { return _groupName; }
    void setGroupName(const char *value)
    {
        strncpy(_groupName, value, 16);
        _groupName[15] = '\0';
    }

    bool getMDNSEnabled(void) { return _mdnsEnabled; }
    void setMDSNEnabled(bool value) { _mdnsEnabled = value; }

    bool getSaveNeeded(void) { return _shouldSaveConfig; }
    void setSaveNeeded(void) { _shouldSaveConfig = true; }

    float getVersion(void) { return _version; }

#pragma region Protected

protected:
    void configPrint(void);
    
    bool _alive;

    char _wifiSSID[32]; // Leave unset for wireless autoconfig. Note that these values will be lost
    char _wifiPass[64]; // when updating, but that's probably OK because they will be saved in EEPROM.

    // These defaults may be overwritten with values saved by the web interface
    // Note that MQTT prefers dotted quad address, but MQTTS prefers fully qualified domain names (fqdn)
    // Note that MQTTS works best using NTP to obtain Time
    char _mqttServer[64];
    char _mqttPort[6];
    char _mqttUser[32];
    char _mqttPassword[32];
    char _nodeName[16];
    char _groupName[16];
    bool _mdnsEnabled;        // mDNS is enabled
    bool _shouldSaveConfig;   // Flag to save json config to SPIFFS
    float _version = VERSION; // Current software release version

#pragma endregion Protected
};