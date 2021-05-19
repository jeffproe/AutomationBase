#include "common.h"
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

// TODO: Class These!
WiFiClient wifiClient; // client for OTA

// a reference to our global copy of self, so we can make working callbacks
extern Esp esp;

// Function implementing callback cannot itself be a class member
static void configSaveCallback()
{ // Callback notifying our config class of the need to save config
    config.saveCallback();
}

// Function implementing callback cannot itself be a class member
void configWiFiCallback(ESP_WiFiManager *myWiFiManager)
{
  debug.printLn(F("WIFI: Failed to connect to assigned AP, entering config mode"));
    while (millis() < 800)
    { // for factory-reset system this will be called before display is responsive. give it a second.
        delay(10);
    }
}

static void resetCallback()
{ // callback to reset the micro
    esp.reset();
}

void Esp::begin()
{                // called in the main code setup, handles our initialisation
    wiFiSetup(); // Start up networking
    // in the original setup() routine, there were other calls here
    // so we have bought setupOTA forward in time...
    setupOta(); // Start OTA firmware update
}

void Esp::loop()
{ // called in the main code loop, handles our periodic code
    while ((WiFi.status() != WL_CONNECTED) || (WiFi.localIP().toString() == "0.0.0.0"))
    { // Check WiFi is connected and that we have a valid IP, retry until we do.
        // NB: tight-looping here breaks code that depends on running periodically, like MQTT clients and ArduinoOTA
        // of course, we have no IP address, so those features are already broken, so this (infinite) loop does not hurt us
        if (WiFi.status() == WL_CONNECTED)
        { // If we're currently connected, disconnect so we can try again
            WiFi.disconnect();
        }
        wiFiReconnect();
    }
}

void Esp::reset()
{
    debug.printLn(F("RESET: ESP reset"));
    mqtt.goodbye();
#ifdef ESP_32
    ESP.restart();
#elif defined(ESP_8266)
    ESP.reset();
#endif
    delay(5000);
}

void Esp::wiFiSetup()
{                                        // Connect to WiFi
    WiFi.macAddress(_espMac);            // Read our MAC address and save it to espMac
    WiFi.hostname(config.getNodeName()); // Assign our hostname before connecting to WiFi
    WiFi.setAutoReconnect(true);         // Tell WiFi to autoreconnect if connection has dropped
    WiFi.setSleep(false); // Disable WiFi sleep modes to prevent occasional disconnects

    if (String(config.getWIFISSID()) == "")
    { // If the sketch has not defined a static wifiSSID use WiFiManager to collect required information from the user.

        // id/name, placeholder/prompt, default value, length, extra tags
        ESP_WMParameter custom_nodeNameHeader("<br/><br/><b>Node Name</b>");
        ESP_WMParameter custom_nodeName("nodeName", "Node (required. lowercase letters, numbers, and _ only)", config.getNodeName(), 15, " maxlength=15 required pattern='[a-z0-9_]*'");
        ESP_WMParameter custom_groupName("groupName", "Group Name (required)", config.getGroupName(), 15, " maxlength=15 required");
        ESP_WMParameter custom_mqttHeader("<br/><br/><b>MQTT Broker</b>");
        ESP_WMParameter custom_mqttServer("mqttServer", "MQTT Server", config.getMQTTServer(), 63, " maxlength=39");
        ESP_WMParameter custom_mqttPort("mqttPort", "MQTT Port", config.getMQTTPort(), 5, " maxlength=5 type='number'");
        ESP_WMParameter custom_mqttUser("mqttUser", "MQTT User", config.getMQTTUser(), 31, " maxlength=31");
        ESP_WMParameter custom_mqttPassword("mqttPassword", "MQTT Password", config.getMQTTPassword(), 31, " maxlength=31 type='password'");
        ESP_WMParameter custom_configHeader("<br/><br/><b>Admin access</b>");
        ESP_WMParameter custom_configUser("configUser", "Config User", web.getUser(), 15, " maxlength=31'");
        ESP_WMParameter custom_configPassword("configPassword", "Config Password", web.getPassword(), 31, " maxlength=31 type='password'");

        ESP_WiFiManager wifiManager;

        wifiManager.setSaveConfigCallback(configSaveCallback); // set config save notify callback
        wifiManager.setCustomHeadElement(web.getStyle());      // add custom style
        wifiManager.addParameter(&custom_nodeNameHeader);
        wifiManager.addParameter(&custom_nodeName);
        wifiManager.addParameter(&custom_groupName);
        wifiManager.addParameter(&custom_mqttHeader);
        wifiManager.addParameter(&custom_mqttServer);
        wifiManager.addParameter(&custom_mqttPort);
        wifiManager.addParameter(&custom_mqttUser);
        wifiManager.addParameter(&custom_mqttPassword);
        wifiManager.addParameter(&custom_configHeader);
        wifiManager.addParameter(&custom_configUser);
        wifiManager.addParameter(&custom_configPassword);

        // Timeout config portal after connectTimeout seconds, useful if configured wifi network was temporarily unavailable
        wifiManager.setTimeout(_connectTimeout);

        wifiManager.setAPCallback(configWiFiCallback);

        // Fetches SSID and pass from EEPROM and tries to connect
        // If it does not connect it starts an access point with the specified name
        // and goes into a blocking loop awaiting configuration.
        if (!wifiManager.autoConnect(_wifiConfigAP, _wifiConfigPass))
        { // Reset and try again
            debug.printLn(WIFI, F("WIFI: Failed to connect and hit timeout"));
            reset();
        }

        // Read updated parameters
        config.setMQTTServer(custom_mqttServer.getValue());
        config.setMQTTPort(custom_mqttPort.getValue());
        config.setMQTTUser(custom_mqttUser.getValue());
        config.setMQTTPassword(custom_mqttPassword.getValue());
        config.setNodeName(custom_nodeName.getValue());
        config.setGroupName(custom_groupName.getValue());
        web.setUser(custom_configUser.getValue());
        web.setPassword(custom_configPassword.getValue());

        config.saveFileIfNeeded();
    }
    else
    { // wifiSSID has been defined, so attempt to connect to it forever
        debug.printLn(WIFI, String(F("Connecting to WiFi network: ")) + String(config.getWIFISSID()));
        WiFi.mode(WIFI_STA);
        WiFi.begin(config.getWIFISSID(), config.getWIFIPass());

        uint32_t wifiReconnectTimer = millis();
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(500);
            if (millis() >= (wifiReconnectTimer + (_connectTimeout * ASECOND)))
            { // If we've been trying to reconnect for connectTimeout seconds, reboot and try again
                debug.printLn(WIFI, F("WIFI: Failed to connect and hit timeout"));
                reset();
            }
        }
    }
    // If you get here you have connected to WiFi
    debug.printLn(WIFI, String(F("WIFI: Connected successfully and assigned IP: ")) + WiFi.localIP().toString());
}

void Esp::wiFiReconnect()
{ // Existing WiFi connection dropped, try to reconnect
    debug.printLn(WIFI, F("Reconnecting to WiFi network..."));
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.getWIFISSID(), config.getWIFIPass());

    uint32_t wifiReconnectTimer = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        if (millis() >= (wifiReconnectTimer + (_reConnectTimeout * ASECOND)))
        { // If we've been trying to reconnect for reConnectTimeout seconds, reboot and try again
            debug.printLn(WIFI, F("WIFI: Failed to reconnect and hit timeout"));
            reset();
        }
    }
}

void Esp::setupOta()
{ // (mostly) boilerplate OTA setup from library examples

    ArduinoOTA.setHostname(config.getNodeName());
    ArduinoOTA.setPassword(web.getPassword());

    ArduinoOTA.onStart([]() {
        debug.printLn(F("ESP OTA: update start"));
    });
    ArduinoOTA.onEnd([]() {
        debug.printLn(F("ESP OTA: update complete"));
        resetCallback();
    });
    ArduinoOTA.onProgress([](uint32_t progress, uint32_t total) {
        // TODO: log something to telnet?
    });
    ArduinoOTA.onError([](ota_error_t error) {
        debug.printLn(String(F("ESP OTA: ERROR code ")) + String(error));
        if (error == OTA_AUTH_ERROR)
            debug.printLn(F("ESP OTA: ERROR - Auth Failed"));
        else if (error == OTA_BEGIN_ERROR)
            debug.printLn(F("ESP OTA: ERROR - Begin Failed"));
        else if (error == OTA_CONNECT_ERROR)
            debug.printLn(F("ESP OTA: ERROR - Connect Failed"));
        else if (error == OTA_RECEIVE_ERROR)
            debug.printLn(F("ESP OTA: ERROR - Receive Failed"));
        else if (error == OTA_END_ERROR)
            debug.printLn(F("ESP OTA: ERROR - End Failed"));
    });
    ArduinoOTA.begin();
    debug.printLn(F("ESP OTA: Over the Air firmware update ready"));
}

String Esp::getMacHex(void)
{ // return our Ethernet MAC Address as 6 hex bytes, 12 characters : "99AABBCCDDEE"
    return String(_espMac[0], HEX) + String(_espMac[1], HEX) + String(_espMac[2], HEX) + String(_espMac[3], HEX) + String(_espMac[4], HEX) + String(_espMac[5], HEX);
    // we could also implement getMacColons to return "99:AA:BB:CC:DD:EE" ?
}

// Submitted by benmprojects to handle "beep" commands. Split
// incoming String by separator, return selected field as String
// Original source: https://arduino.stackexchange.com/a/1237
String Esp::getSubtringField(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = {0, -1};
    int maxIndex = data.length();

    for (int i = 0; i <= maxIndex && found <= index; i++)
    {
        if (data.charAt(i) == separator || i == maxIndex)
        {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i + 1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

String Esp::printHex8(uint8_t *data, uint8_t length)
{ // returns input bytes as printable hex values in the format 01 23 FF

    String hex8String;
    for (int i = 0; i < length; i++)
    {
        // hex8String += "0x";
        if (data[i] < 0x10)
        {
            hex8String += "0";
        }
        hex8String += String(data[i], HEX);
        if (i != (length - 1))
        {
            hex8String += " ";
        }
    }
    hex8String.toUpperCase();
    return hex8String;
}
