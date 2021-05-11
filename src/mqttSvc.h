#pragma once

#include "settings.h"
#include <Arduino.h>

class MqttSvc
{
#pragma region Private

private:
#pragma endregion Private

#pragma region Public

public:
    // constructor
    MqttSvc(void) { _alive = false; }

    // destructor
    ~MqttSvc(void) { _alive = false; }

    void begin();
    void loop();
    void connect();
    void callback(String &strTopic, String &strPayload);
    void statusUpdate();
    bool clientIsConnected();
    String clientReturnCode();
    void publishStatusTopic(String msg);
    void publishStateTopic(String msg);
    void publishButtonEvent(String page, String buttonID, String newState);
    void publishButtonJSONEvent(String page, String buttonID, String newState);
    void publishStatePage(String page);
    void publishStateSubTopic(String subtopic, String newState);
    String getClientID(void);
    uint16_t getMaxPacketSize(void);
    void goodbye();

#pragma endregion Public

#pragma region Protected

protected:
    const uint32_t _statusUpdateInterval = MQTT_STATUS_UPDATE_INTERVAL; // Time in msec between publishing MQTT status updates (5 minutes)
    const uint32_t _mqttConnectTimeout = CONNECTION_TIMEOUT;            // Timeout for WiFi and MQTT connection attempts in seconds

    bool _alive;                 // Flag that data structures are initialised and functions can run without error
    String _clientId;            // Auto-generated MQTT ClientID
    String _getSubtopic;         // MQTT subtopic for incoming commands requesting .val
    String _getSubtopicJSON;     // MQTT object buffer for JSON status when requesting .val
    String _stateTopic;          // MQTT topic for outgoing panel interactions
    String _stateJSONTopic;      // MQTT topic for outgoing panel interactions in JSON format
    String _commandTopic;        // MQTT topic for incoming panel commands
    String _groupCommandTopic;   // MQTT topic for incoming group panel commands
    String _statusTopic;         // MQTT topic for publishing device connectivity state
    String _sensorTopic;         // MQTT topic for publishing device information in JSON format
    uint32_t _statusUpdateTimer; // Timer for update check

#pragma endregion Protected
};