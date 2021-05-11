// config_class.cpp : Class internals to support our configuration controls and JSON file on a SPIFFS Partition
//
// ----------------------------------------------------------------------------------------------------------------- //

// NB: after ESPCore 2.6.3, SPIFFS is deprecated in favour of LittleFS

#include "common.h"
#include <ArduinoJson.h>
#include <FS.h> // spiffs is deprecated after 2.6.3, littleFS is new
#include <EEPROM.h>
#ifdef ESP_32
#include <ArduinoNvs.h>
#endif

void Config::begin()
{ // called in the main code setup, handles our initialisation
  _alive = true;
#if !DISABLE_CONFIG_READ
  readFile();
#endif
}

void Config::loop()
{ // called in the main code loop, handles our periodic code
}

void Config::readFile()
{
#ifdef ESP_32
  debug.printLn(F("NVS: reading from NVS"));
  if (NVS.begin())
  {
    char configPassword[32];
    char configUser[32];
    int debugTelnetEnabled;
    int mdnsEnabled;

    NVS.getString("nodeName").toCharArray(_nodeName, sizeof(_nodeName));
    NVS.getString("groupName").toCharArray(_groupName, sizeof(_groupName));
    NVS.getString("mqttServer").toCharArray(_mqttServer, sizeof(_mqttServer));
    NVS.getString("mqttPort").toCharArray(_mqttPort, sizeof(_mqttPort));
    NVS.getString("mqttUser").toCharArray(_mqttUser, sizeof(_mqttUser));
    NVS.getString("mqttPassword").toCharArray(_mqttPassword, sizeof(_mqttPassword));
    NVS.getString("configPassword").toCharArray(configPassword, sizeof(configPassword));
    NVS.getString("configUser").toCharArray(configUser, sizeof(configUser));
    web.setUser(configUser);
    debugTelnetEnabled = NVS.getInt("debugTelnetEnabled");
    debug.enableTelnet(debugTelnetEnabled == 1);
    mdnsEnabled = NVS.getInt("mdnsEnabled");
    setMDSNEnabled(mdnsEnabled == 1);

    configPrint();
  }
  else
  {
    debug.printLn(F("NVS: [ERROR] Failed to start NVS"));
  }
#elif defined(ESP_8266)
  // Read saved config.json from SPIFFS
  debug.printLn(F("SPIFFS: mounting SPIFFS"));
  if (SPIFFS.begin())
  {
    if (SPIFFS.exists("/config.json"))
    { // File exists, reading and loading
      debug.printLn(F("SPIFFS: reading /config.json"));
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile)
      {
        size_t configFileSize = configFile.size(); // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[configFileSize]);
        configFile.readBytes(buf.get(), configFileSize);

        DynamicJsonDocument configJson(1024);
        DeserializationError jsonError = deserializeJson(configJson, buf.get());
        if (jsonError)
        { // Couldn't parse the saved config
          debug.printLn(String(F("SPIFFS: [ERROR] Failed to parse /config.json: ")) + String(jsonError.c_str()));
        }
        else
        {
          if (!configJson["mqttServer"].isNull())
          {
            setMQTTServer(configJson["mqttServer"]);
          }
          if (!configJson["mqttPort"].isNull())
          {
            setMQTTPort(configJson["mqttPort"]);
          }
          if (!configJson["mqttUser"].isNull())
          {
            setMQTTUser(configJson["mqttUser"]);
          }
          if (!configJson["mqttPassword"].isNull())
          {
            setMQTTPassword(configJson["mqttPassword"]);
          }
          if (!configJson["nodeName"].isNull())
          {
            setNodeName(configJson["nodeName"]);
          }
          if (!configJson["groupName"].isNull())
          {
            setGroupName(configJson["groupName"]);
          }
          if (!configJson["configUser"].isNull())
          {
            web.setUser(configJson["configUser"]);
          }
          if (!configJson["configPassword"].isNull())
          {
            web.setPassword(configJson["configPassword"]);
          }
          if (!configJson["debugTelnetEnabled"].isNull())
          {
            debug.enableTelnet(configJson["debugTelnetEnabled"]); // debug, config, or both?
          }
          if (!configJson["mdnsEnabled"].isNull())
          {
            setMDSNEnabled(configJson["mdnsEnabled"]);
          }
          String configJsonStr;
          serializeJson(configJson, configJsonStr);
          debug.printLn(String(F("SPIFFS: parsed json:")) + configJsonStr);
        }
      }
      else
      {
        debug.printLn(F("SPIFFS: [ERROR] Failed to read /config.json"));
      }
    }
    else
    {
      debug.printLn(F("SPIFFS: [WARNING] /config.json not found, will be created on first config save"));
    }
  }
  else
  {
    debug.printLn(F("SPIFFS: [ERROR] Failed to mount FS"));
  }
#endif
}

void Config::saveCallback()
{ // Callback notifying us of the need to save config
  debug.printLn(F("SPIFFS: Configuration changed, flagging for save"));
  _shouldSaveConfig = true;
}

void Config::saveFileIfNeeded()
{ // if we should save, do save
  if (_shouldSaveConfig)
  {
    saveFile();
  }
}

void Config::saveFile()
{
#ifdef ESP_32
  // Save the custom parameters to NVS
  debug.printLn(F("NVS: Saving config"));

  NVS.setString("nodeName", _nodeName);
  NVS.setString("groupName", _groupName);
  NVS.setString("mqttServer", _mqttServer);
  NVS.setString("mqttPort", _mqttPort);
  NVS.setString("mqttUser", _mqttUser);
  NVS.setString("mqttPassword", _mqttPassword);
  NVS.setString("configPassword", web.getPassword());
  NVS.setString("configUser", web.getPassword());
  NVS.setInt("debugTelnetEnabled", debug.getTelnetEnabled() ? 1 : 0);
  NVS.setInt("mdnsEnabled", getMDNSEnabled() ? 1 : 0);

  NVS.commit();

  configPrint();
#elif defined(ESP_8266)
  // Save the custom parameters to config.json
  debug.printLn(F("SPIFFS: Saving config"));
  DynamicJsonDocument jsonConfigValues(1024);
  jsonConfigValues["mqttServer"] = _mqttServer;
  jsonConfigValues["mqttPort"] = _mqttPort;
  jsonConfigValues["mqttUser"] = _mqttUser;
  jsonConfigValues["mqttPassword"] = _mqttPassword;
  jsonConfigValues["nodeName"] = _nodeName;
  jsonConfigValues["groupName"] = _groupName;
  jsonConfigValues["configUser"] = web.getUser();
  jsonConfigValues["configPassword"] = web.getPassword();
  jsonConfigValues["debugTelnetEnabled"] = debug.getTelnetEnabled();
  jsonConfigValues["mdnsEnabled"] = _mdnsEnabled;

  debug.printLn(String(F("SPIFFS: mqttServer = ")) + String(_mqttServer));
  debug.printLn(String(F("SPIFFS: mqttPort = ")) + String(_mqttPort));
  debug.printLn(String(F("SPIFFS: mqttUser = ")) + String(_mqttUser));
  debug.printLn(String(F("SPIFFS: mqttPassword = ")) + String(_mqttPassword));
  debug.printLn(String(F("SPIFFS: nodeName = ")) + String(_nodeName));
  debug.printLn(String(F("SPIFFS: groupName = ")) + String(_groupName));
  debug.printLn(String(F("SPIFFS: configUser = ")) + String(web.getUser()));
  debug.printLn(String(F("SPIFFS: configPassword = ")) + String(web.getPassword()));
  debug.printLn(String(F("SPIFFS: debugTelnetEnabled = ")) + String(debug.getTelnetEnabled()));
  debug.printLn(String(F("SPIFFS: mdnsEnabled = ")) + String(_mdnsEnabled));

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile)
  {
    debug.printLn(F("SPIFFS: Failed to open config file for writing"));
  }
  else
  {
    serializeJson(jsonConfigValues, configFile);
    configFile.close();
  }
#endif
  _shouldSaveConfig = false;
}

void Config::clearFileSystem()
{ // Clear out all local storage
#ifdef ESP_32
  debug.printLn(F("RESET: Formatting NVS"));
  NVS.eraseAll();
#elif defined(ESP_8266)
  debug.printLn(F("RESET: Formatting SPIFFS"));
  SPIFFS.format();
#endif
  web.resetWifiManager();
  EEPROM.begin(512);
  debug.printLn(F("Clearing EEPROM..."));
  for (uint16_t i = 0; i < EEPROM.length(); i++)
  {
    EEPROM.write(i, 0);
  }
  debug.printLn(F("RESET: Rebooting device"));
  esp.reset();
}

void Config::configPrint()
{
  debug.printLn(String(F("NVS: mqttServer = ")) + String(_mqttServer));
  debug.printLn(String(F("NVS: mqttPort = ")) + String(_mqttPort));
  debug.printLn(String(F("NVS: mqttUser = ")) + String(_mqttUser));
  debug.printLn(String(F("NVS: mqttPassword = ")) + String(_mqttPassword));
  debug.printLn(String(F("NVS: nodeName = ")) + String(_nodeName));
  debug.printLn(String(F("NVS: groupName = ")) + String(_groupName));
  debug.printLn(String(F("NVS: configUser = ")) + String(web.getUser()));
  debug.printLn(String(F("NVS: configPassword = ")) + String(web.getPassword()));
  debug.printLn(String(F("NVS: debugTelnetEnabled = ")) + String(debug.getTelnetEnabled()));
  debug.printLn(String(F("NVS: mdnsEnabled = ")) + String(_mdnsEnabled));
}
