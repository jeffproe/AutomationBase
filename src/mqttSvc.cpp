#include "common.h"
#include <MQTT.h>
#include <ArduinoOTA.h>

// Because the mqttClient object is defined outside the class, then the constant it uses must also be outside the class.
static const uint16_t _mqttMaxPacketSize = MQTT_MAX_PACKET_SIZE; // Size of buffer for incoming MQTT message

WiFiClient wifiMQTTClient;                 // client for MQTT
MQTTClient mqttClient(_mqttMaxPacketSize); // MQTT Object

#pragma region Callbacks

// callback prototype is "typedef void (*MQTTClientCallbackSimple)(String &topic, String &payload)"
// So we cannot declare our callback within the class, as it gets the wrong prototype
// So we have our callback outside the class and then have it call into the (global) class
// to do the actual work of parsing the mqtt message
// and yes, we need a local copy of "self" to handle our callbacks.
void mqtt_callback(String &strTopic, String &strPayload)
{
    mqtt.callback(strTopic, strPayload);
}

#pragma endregion Callbacks

void MqttSvc::begin()
{ // called in the main code setup, handles our initialisation
    _alive = true;
    _statusUpdateTimer = 0;
    mqttClient.begin(config.getMQTTServer(), atoi(config.getMQTTPort()), wifiMQTTClient); // Create MQTT service object
    mqttClient.onMessage(mqtt_callback);                                                  // Setup MQTT callback function
    connect();                                                                            // Connect to MQTT
}

void MqttSvc::loop()
{ // called in the main code loop, handles our periodic code
    if (!_alive)
    {
        begin();
    }
    if (!mqttClient.connected())
    { // Check MQTT connection
        debug.printLn("MQTT: not connected, connecting.");
        connect();
    }

    mqttClient.loop(); // MQTT client loop
    if ((millis() - _statusUpdateTimer) >= _statusUpdateInterval)
    { // Run periodic status update
        statusUpdate();
    }
}

void MqttSvc::connect()
{ // MQTT connection and subscriptions

    static bool mqttFirstConnect = true; // For the first connection, we want to send an OFF/ON state to
    // trigger any automations, but skip that if we reconnect while
    // still running the sketch

    // Check to see if we have a broker configured and notify the user if not
    if (config.getMQTTServer()[0] == 0) // this check could be more elegant, eh?
    {
        while (config.getMQTTServer()[0] == 0)
        { // Handle HTTP and OTA while we're waiting for MQTT to be configured
            yield();
            web.loop();
            ArduinoOTA.handle(); // TODO: move this elsewhere!
        }
    }
    // MQTT topic string definitions
    _stateTopic = "esp/" + String(config.getNodeName()) + "/state";
    _stateJSONTopic = "esp/" + String(config.getNodeName()) + "/state/json";
    _commandTopic = "esp/" + String(config.getNodeName()) + "/command";
    _groupCommandTopic = "esp/" + String(config.getGroupName()) + "/command";
    _statusTopic = "esp/" + String(config.getNodeName()) + "/status";
    _sensorTopic = "esp/" + String(config.getNodeName()) + "/sensor";

    const String commandSubscription = _commandTopic + "/#";
    const String groupCommandSubscription = _groupCommandTopic + "/#";
    
    // Loop until we're reconnected to MQTT
    while (!mqttClient.connected())
    {
        // Create a reconnect counter
        static uint8_t mqttReconnectCount = 0;

        // Generate an MQTT client ID as nodeName + our MAC address
        _clientId = String(config.getNodeName()) + "-" + esp.getMacHex();
        debug.printLn(String(F("MQTT: Attempting connection to broker ")) + String(config.getMQTTServer()) + " as clientID " + _clientId);

        // Set keepAlive, cleanSession, timeout
        mqttClient.setOptions(30, true, 5000);

        // declare LWT
        mqttClient.setWill(_statusTopic.c_str(), "OFF");

        if (mqttClient.connect(_clientId.c_str(), config.getMQTTUser(), config.getMQTTPassword()))
        { // Attempt to connect to broker, setting last will and testament
            // Subscribe to our incoming topics
            if (mqttClient.subscribe(commandSubscription))
            {
                debug.printLn(String(F("MQTT: subscribed to ")) + commandSubscription);
            }
            if (mqttClient.subscribe(groupCommandSubscription))
            {
                debug.printLn(String(F("MQTT: subscribed to ")) + groupCommandSubscription);
            }
            if (mqttClient.subscribe(_statusTopic))
            {
                debug.printLn(String(F("MQTT: subscribed to ")) + _statusTopic);
            }

            if (mqttFirstConnect)
            { // Force any subscribed clients to toggle OFF/ON when we first connect.  Sending OFF,
                // "ON" will be sent by the _statusTopic subscription action.
                debug.printLn(String(F("MQTT: binary_sensor state: [")) + _statusTopic + "] : [OFF]");
                mqttClient.publish(_statusTopic, "OFF", true, 1);
                mqttFirstConnect = false;
            }
            else
            {
                debug.printLn(String(F("MQTT: binary_sensor state: [")) + _statusTopic + "] : [ON]");
                mqttClient.publish(_statusTopic, "ON", true, 1);
            }

            mqttReconnectCount = 0;

            debug.printLn(F("MQTT: connected"));
        }
        else
        { // Retry until we give up and restart after mqttConnectTimeout seconds
            mqttReconnectCount++;
            if (mqttReconnectCount > ((_mqttConnectTimeout / 10) - 1))
            {
                debug.printLn(String(F("MQTT connection attempt ")) + String(mqttReconnectCount) + String(F(" failed with rc ")) + String(mqttClient.returnCode()) + String(F(".  Restarting device.")));
                esp.reset();
            }
            debug.printLn(String(F("MQTT connection attempt ")) + String(mqttReconnectCount) + String(F(" failed with rc ")) + String(mqttClient.returnCode()) + String(F(".  Trying again in 30 seconds.")));
            uint32_t mqttReconnectTimer = millis(); // record current time for our timeout
            while ((millis() - mqttReconnectTimer) < 30000)
            { // Handle HTTP and OTA while we're waiting 30sec for MQTT to reconnect
                web.loop();
                ArduinoOTA.handle();
                delay(10);
            }
        }
    }
}

void MqttSvc::callback(String &strTopic, String &strPayload)
{ // Handle incoming commands from MQTT
    debug.printLn(MQTT, String(F("MQTT IN: '")) + strTopic + "' : '" + strPayload + "'");

    if (((strTopic == _commandTopic) || (strTopic == _groupCommandTopic)) && (strPayload == ""))
    {                   // '[...]/device/command' -m '' = No command requested, respond with statusUpdate()
        statusUpdate(); // return status JSON via MQTT
    }
    else if (strTopic == (_commandTopic + "/statusupdate") || strTopic == (_groupCommandTopic + "/statusupdate"))
    {                   // '[...]/device/command/statusupdate' == mqttStatusUpdate()
        statusUpdate(); // return status JSON via MQTT
    }
    else if (strTopic == (_commandTopic + "/reboot") || strTopic == (_groupCommandTopic + "/reboot"))
    { // '[...]/device/command/reboot' == reboot microcontroller)
        debug.printLn(F("MQTT: Rebooting device"));
        esp.reset();
    }
    else if (strTopic == (_commandTopic + "/factoryreset") || strTopic == (_groupCommandTopic + "/factoryreset"))
    { // '[...]/device/command/factoryreset' == clear all saved settings)
        config.clearFileSystem();
    }
    else if (strTopic == _statusTopic && strPayload == "OFF")
    { // catch a dangling LWT from a previous connection if it appears
        mqttClient.publish(_statusTopic, "ON");
    }
}

void MqttSvc::statusUpdate()
{ // Periodically publish a JSON string indicating system status
    _statusUpdateTimer = millis();
    String statusPayload = "{";
    statusPayload += String(F("\"status\":\"available\","));
    statusPayload += String(F("\"espVersion\":")) + String(VERSION) + String(F(","));
    statusPayload += String(F("\"espUptime\":")) + String(int32_t(millis() / 1000)) + String(F(","));
    statusPayload += String(F("\"signalStrength\":")) + String(WiFi.RSSI()) + String(F(","));
    statusPayload += String(F("\"IP\":\"")) + WiFi.localIP().toString() + String(F("\","));
    statusPayload += String(F("\"heapFree\":")) + String(ESP.getFreeHeap()) + String(F(","));
    #ifdef ESP_32
    statusPayload += String(F("\"espSdk\":\"")) + String(ESP.getSdkVersion()) + String(F("\""));
    #elif defined(ESP_8266)
    statusPayload += String(F("\"heapFragmentation\":")) + String(ESP.getHeapFragmentation()) + String(F(","));
    statusPayload += String(F("\"espCore\":\"")) + String(ESP.getCoreVersion()) + String(F("\""));
    #endif
    statusPayload += "}";

    mqttClient.publish(_sensorTopic, statusPayload, true, 1);
    mqttClient.publish(_statusTopic, "ON", true, 1);
    debug.printLn(String(F("MQTT: status update: ")) + String(statusPayload));
    debug.printLn(String(F("MQTT: binary_sensor state: [")) + _statusTopic + "] : [ON]");
}

bool MqttSvc::clientIsConnected() { return mqttClient.connected(); }

String MqttSvc::clientReturnCode() { return String(mqttClient.returnCode()); }

String MqttSvc::getClientID() { return _clientId; }

void MqttSvc::publishStateTopic(String msg) { mqttClient.publish(_stateTopic, msg); }

void MqttSvc::publishStatusTopic(String msg) { mqttClient.publish(_statusTopic, msg); }

void MqttSvc::publishButtonEvent(String page, String buttonID, String newState)
{ // Publish a message that buttonID on page is now newState
    String mqttButtonTopic = _stateTopic + "/p[" + page + "].b[" + buttonID + "]";
    mqttClient.publish(mqttButtonTopic, newState);
    debug.printLn(MQTT, String(F("MQTT OUT: '")) + mqttButtonTopic + "' : '" + newState + "'");
}

void MqttSvc::publishButtonJSONEvent(String page, String buttonID, String newState)
{ // Publish a JSON message stating button = newState, on the State JSON Topic
    String mqttButtonJSONEvent = String(F("{\"event\":\"p[")) + String(page) + String(F("].b[")) + String(buttonID) + String(F("]\", \"value\":\"")) + newState + String(F(""
                                                                                                                                                                           "}"));
    mqttClient.publish(_stateJSONTopic, mqttButtonJSONEvent);
}

void MqttSvc::publishStatePage(String page)
{ // Publish a page message on the State Topic
    String mqttPageTopic = _stateTopic + "/page";
    mqttClient.publish(mqttPageTopic, page);
    debug.printLn(MQTT, String(F("MQTT OUT: '")) + mqttPageTopic + "' : '" + page + "'");
}

void MqttSvc::publishStateSubTopic(String subtopic, String newState)
{ // extend the State Topic with a subtopic and publish a newState message on it
    String mqttReturnTopic = _stateTopic + subtopic;
    mqttClient.publish(mqttReturnTopic, newState);
    debug.printLn(MQTT, String(F("MQTT OUT: '")) + mqttReturnTopic + "' : '" + newState + "]");
}


uint16_t MqttSvc::getMaxPacketSize(void)
{ // return the (non-class) variable for our network buffer. See note at the top of mqtt_class.cpp
    return _mqttMaxPacketSize;
}

void MqttSvc::goodbye()
{ // like a Last-Will-and-Testament, publish something when we are going offline
    if (mqttClient.connected())
    {
        mqttClient.publish(_statusTopic, "OFF", true, 1);
        mqttClient.publish(_sensorTopic, "{\"status\": \"unavailable\"}", true, 1);
        mqttClient.disconnect();
    }
}
