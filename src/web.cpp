#include "common.h"
#ifdef ESP_32
#include <WebServer.h>
#include <ESPmDNS.h>
#elif defined(ESP_8266)
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h> // MDNSResponder
#endif

static const uint32_t telnetInputMax = 128; // Size of user input buffer for user telnet session

#ifdef ESP_32
WebServer webServer(80);
#elif defined(ESP_8266)
ESP8266WebServer webServer(80);           // Server listening for HTTP
#endif

WiFiServer telnetServer(23); // Server listening for Telnet
WiFiClient telnetClient;

#ifdef ESP_32
// ESP32
#elif defined(ESP_8266)
MDNSResponder::hMDNSService hMDNSService; // Bonjour
#endif

// a reference to our global copy of self, so we can make working callbacks
extern Web web;

#pragma region Callbacks
// callback prototype is "std::function<void ()> handler"
// So we cannot declare our callback within the class, as it gets the wrong prototype
// So we have our callback outside the class and then have it call into the (global) class
// to do the actual work of parsing the http message
// and yes, we need a local copy of "self" to handle our callbacks.
void callback_HandleNotFound()
{
  web._handleNotFound();
}
void callback_HandleRoot()
{
  web._handleRoot();
}
void callback_HandleSaveConfig()
{
  web._handleSaveConfig();
}
void callback_HandleResetConfig()
{
  web._handleResetConfig();
}
void callback_HandleReboot()
{
  web._handleReboot();
}

#pragma endregion Callbacks

void Web::begin()
{ // called in the main code setup, handles our initialisation
  _alive = true;

  setUser(DEFAULT_CONFIG_USER);
  setPassword(DEFAULT_CONFIG_PASS);
  _tftFileSize = 0;

  _setupHTTP();

  if (config.getMDNSEnabled())
  { // Setup mDNS service discovery if enabled
    _setupMDNS();
  }

  if (debug.getTelnetEnabled())
  { // Setup telnet server for remote debug output
    _setupTelnet();
  }
}

void Web::loop()
{ // called in the main code loop, handles our periodic code
  if (!_alive)
  {
    begin();
  }
  webServer.handleClient(); // webServer loop

  if (config.getMDNSEnabled())
  {
#ifdef ESP_32
// ESP32
// TODO: how do I send an update out with ESP32?
#elif defined(ESP_8266)
    MDNS.update();
#endif
  }

  if (debug.getTelnetEnabled())
  {
    _handleTelnetClient(); // telnetClient loop
  }
}

void Web::resetWifiManager()
{
  #ifdef ESP_32
    debug.printLn(F("RESET: Clearing WiFiManager settings..."));
  ESP_WiFiManager wifiManager;
  wifiManager.resetSettings();
  #elif defined(ESP_8266)
  debug.printLn(F("RESET: Clearing WiFiManager settings..."));
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  #endif
}

void Web::_setupHTTP()
{
  webServer.on("/", callback_HandleRoot);
  webServer.on("/saveConfig", callback_HandleSaveConfig);
  webServer.on("/resetConfig", callback_HandleResetConfig);
  webServer.on("/reboot", callback_HandleReboot);
  webServer.onNotFound(callback_HandleNotFound);
  webServer.begin();
  debug.printLn(String(F("HTTP: Server started @ http://")) + WiFi.localIP().toString());
}

void Web::_setupMDNS()
{
#ifdef ESP_32
  if(!MDNS.begin(config.getNodeName()))
  {
    debug.printLn(String(F("MDNS: Init failed!")));
    return;
  }

  //set default instance
  MDNS.setInstanceName("ESPDevice");

  MDNS.addService("_http", "_tcp", 80);
  if (debug.getTelnetEnabled())
  {
    MDNS.addService("_telnet", "_tcp", 23);
  }
  MDNS.addServiceTxt("_http", "_tcp", "app_name", "ESPDevice");
  MDNS.addServiceTxt("_http", "_tcp", "app_version", String(config.getVersion()).c_str());
#elif defined(ESP_8266)
  hMDNSService = MDNS.addService(config.getNodeName(), "http", "tcp", 80);
  if (debug.getTelnetEnabled())
  {
    MDNS.addService(config.getNodeName(), "telnet", "tcp", 23);
  }
  MDNS.addServiceTxt(hMDNSService, "app_name", "ESPDevice");
  MDNS.addServiceTxt(hMDNSService, "app_version", String(config.getVersion()).c_str());
  MDNS.update();
#endif
}

void Web::_setupTelnet()
{
  telnetServer.setNoDelay(true);
  telnetServer.begin();
  debug.printLn(String(F("TELNET: debug server enabled at telnet:")) + WiFi.localIP().toString());
}

void Web::_handleTelnetClient()
{ // Basic telnet client handling code from: https://gist.github.com/tablatronix/4793677ca748f5f584c95ec4a2b10303
  static uint32_t telnetInputIndex = 0;
  if (telnetServer.hasClient())
  { // client is connected
    if (!telnetClient || !telnetClient.connected())
    {
      if (telnetClient)
      {
        telnetClient.stop(); // client disconnected
      }
      telnetClient = telnetServer.available(); // ready for new client
      telnetInputIndex = 0;                    // reset input buffer index
    }
    else
    {
      telnetServer.available().stop(); // have client, block new connections
    }
  }
  // Handle client input from telnet connection.
  if (telnetClient && telnetClient.connected() && telnetClient.available())
  { // client input processing
    static char telnetInputBuffer[telnetInputMax];

    if (telnetClient.available())
    {
      char telnetInputByte = telnetClient.read(); // Read client byte
      // debug.printLn(String("telnet in: 0x") + String(telnetInputByte, HEX));
      if (telnetInputByte == 5)
      { // If the telnet client sent a bunch of control commands on connection (which end in ENQUIRY/0x05), ignore them and restart the buffer
        telnetInputIndex = 0;
      }
      else if (telnetInputByte == 13)
      { // telnet line endings should be CRLF: https://tools.ietf.org/html/rfc5198#appendix-C
        // If we get a CR just ignore it
      }
      else if (telnetInputIndex < telnetInputMax)
      { // If we have room left in our buffer add the current byte
        telnetInputBuffer[telnetInputIndex] = telnetInputByte;
        telnetInputIndex++;
      }
    }
  }
}

void Web::telnetPrintLn(bool enabled, String message)
{
  if (enabled && telnetClient.connected())
  {
    telnetClient.println(message);
  }
}

void Web::telnetPrint(bool enabled, String message)
{
  if (enabled && telnetClient.connected())
  {
    telnetClient.print(message);
  }
}

bool Web::_authenticated(void)
{ // common code to verify our authentication on most handle callbacks
  if (_configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(_configUser, _configPassword))
    {
      webServer.requestAuthentication();
      return false;
    }
  }
  // authentication passes or not required
  return true;
}

void Web::_handleNotFound()
{ // webServer 404
  debug.printLn(String(F("HTTP: Sending 404 to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = "File Not Found\n\n";
  httpMessage += "URI: ";
  httpMessage += webServer.uri();
  httpMessage += "\nMethod: ";
  httpMessage += (webServer.method() == HTTP_GET) ? "GET" : "POST";
  httpMessage += "\nArguments: ";
  httpMessage += webServer.args();
  httpMessage += "\n";
  for (uint8_t i = 0; i < webServer.args(); i++)
  {
    httpMessage += " " + webServer.argName(i) + ": " + webServer.arg(i) + "\n";
  }
  webServer.send(404, "text/plain", httpMessage);
}

void Web::_handleRoot()
{ // http://ESP01/
  if (!_authenticated())
  {
    return;
  }

  debug.printLn(String(F("HTTP: Sending root page to client connected from: ")) + webServer.client().remoteIP().toString());
#ifdef ESP_32
  String httpMessage = FPSTR(WM_HTTP_HEAD_START);
#elif defined(ESP_8266)
  String httpMessage = FPSTR(HTTP_HEADER);
#endif
  httpMessage.replace("{v}", String(config.getNodeName()));
#ifdef ESP_32
  httpMessage += FPSTR(WM_HTTP_SCRIPT);
  httpMessage += FPSTR(WM_HTTP_STYLE);
#elif defined(ESP_8266)
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
#endif
  httpMessage += String(_style);
#ifdef ESP_32
  httpMessage += FPSTR(WM_HTTP_HEAD_END);
#elif defined(ESP_8266)
  httpMessage += FPSTR(HTTP_HEADER_END);
#endif
  httpMessage += String(F("<h1>"));
  httpMessage += String(config.getNodeName());
  httpMessage += String(F("</h1>"));

  httpMessage += String(F("<form method='POST' action='saveConfig'>"));
  httpMessage += String(F("<b>WiFi SSID</b> <i><small>(required)</small></i><input id='wifiSSID' required name='wifiSSID' maxlength=32 placeholder='WiFi SSID' value='")) + String(WiFi.SSID()) + "'>";
  httpMessage += String(F("<br/><b>WiFi Password</b> <i><small>(required)</small></i><input id='wifiPass' required name='wifiPass' type='password' maxlength=64 placeholder='WiFi Password' value='")) + String("********") + "'>";
  httpMessage += String(F("<br/><br/><b>Node Name</b> <i><small>(required. lowercase letters, numbers, and _ only)</small></i><input id='nodeName' required name='nodeName' maxlength=15 placeholder='Node Name' pattern='[a-z0-9_]*' value='")) + String(config.getNodeName()) + "'>";
  httpMessage += String(F("<br/><br/><b>Group Name</b> <i><small>(required)</small></i><input id='groupName' required name='groupName' maxlength=15 placeholder='Group Name' value='")) + String(config.getGroupName()) + "'>";
  httpMessage += String(F("<br/><br/><b>MQTT Broker</b> <i><small>(required)</small></i><input id='mqttServer' required name='mqttServer' maxlength=63 placeholder='mqttServer' value='")) + String(config.getMQTTServer()) + "'>";
  httpMessage += String(F("<br/><b>MQTT Port</b> <i><small>(required)</small></i><input id='mqttPort' required name='mqttPort' type='number' maxlength=5 placeholder='mqttPort' value='")) + String(config.getMQTTPort()) + "'>";
  httpMessage += String(F("<br/><b>MQTT User</b> <i><small>(optional)</small></i><input id='mqttUser' name='mqttUser' maxlength=31 placeholder='mqttUser' value='")) + String(config.getMQTTUser()) + "'>";
  httpMessage += String(F("<br/><b>MQTT Password</b> <i><small>(optional)</small></i><input id='mqttPassword' name='mqttPassword' type='password' maxlength=31 placeholder='mqttPassword' value='"));
  if (strlen(config.getMQTTPassword()) != 0)
  {
    httpMessage += String("********");
  }
  httpMessage += String(F("'><br/><br/><b>Admin Username</b> <i><small>(optional)</small></i><input id='configUser' name='configUser' maxlength=31 placeholder='Admin User' value='")) + String(_configUser) + "'>";
  httpMessage += String(F("<br/><b>Admin Password</b> <i><small>(optional)</small></i><input id='configPassword' name='configPassword' type='password' maxlength=31 placeholder='Admin User Password' value='"));
  if (strlen(_configPassword) != 0)
  {
    httpMessage += String("********");
  }

  httpMessage += String(F("><br/><b>Telnet debug output enabled:</b><input id='debugTelnetEnabled' name='debugTelnetEnabled' type='checkbox'"));
  if (debug.getTelnetEnabled())
  {
    httpMessage += String(F(" checked='checked'"));
  }
  httpMessage += String(F("><br/><b>mDNS enabled:</b><input id='mdnsEnabled' name='mdnsEnabled' type='checkbox'"));
  if (config.getMDNSEnabled())
  {
    httpMessage += String(F(" checked='checked'"));
  }

  httpMessage += String(F("><br/><hr><button type='submit'>save settings</button></form>"));

  httpMessage += String(F("<hr><form method='get' action='reboot'>"));
  httpMessage += String(F("<button type='submit'>reboot device</button></form>"));

  httpMessage += String(F("<hr><form method='get' action='resetConfig'>"));
  httpMessage += String(F("<button type='submit'>factory reset settings</button></form>"));

  httpMessage += String(F("<hr><b>MQTT Status: </b>"));
  if (mqtt.clientIsConnected())
  { // Check MQTT connection
    httpMessage += String(F("Connected"));
  }
  else
  {
    httpMessage += String(F("<font color='red'><b>Disconnected</b></font>, return code: ")) + mqtt.clientReturnCode();
  }
  httpMessage += String(F("<br/><b>MQTT ClientID: </b>")) + mqtt.getClientID();
  httpMessage += String(F("<br/><b>Version: </b>")) + String(config.getVersion());
  httpMessage += String(F("<br/><b>CPU Frequency: </b>")) + String(ESP.getCpuFreqMHz()) + String(F("MHz"));
  httpMessage += String(F("<br/><b>Sketch Size: </b>")) + String(ESP.getSketchSize()) + String(F(" bytes"));
  httpMessage += String(F("<br/><b>Free Sketch Space: </b>")) + String(ESP.getFreeSketchSpace()) + String(F(" bytes"));
  httpMessage += String(F("<br/><b>Heap Free: </b>")) + String(ESP.getFreeHeap());
#ifdef ESP_32
  httpMessage += String(F("<br/><b>ESP sdk version: </b>")) + String(ESP.getSdkVersion());
#elif defined(ESP_8266)
  httpMessage += String(F("<br/><b>Heap Fragmentation: </b>")) + String(ESP.getHeapFragmentation());
  httpMessage += String(F("<br/><b>ESP core version: </b>")) + String(ESP.getCoreVersion());
#endif
  httpMessage += String(F("<br/><b>IP Address: </b>")) + String(WiFi.localIP().toString());
  httpMessage += String(F("<br/><b>Signal Strength: </b>")) + String(WiFi.RSSI());
  httpMessage += String(F("<br/><b>Uptime: </b>")) + String(int32_t(millis() / 1000));
#ifdef ESP_32
  httpMessage += String(F("<br/><b>Last reset: </b>")) + String((int)rtc_get_reset_reason(0));
#elif defined(ESP_8266)
  httpMessage += String(F("<br/><b>Last reset: </b>")) + String(ESP.getResetInfo());
#endif

#ifdef ESP_32
  httpMessage += FPSTR(WM_HTTP_END);
#elif defined(ESP_8266)
  httpMessage += FPSTR(HTTP_END);
#endif
  webServer.send(200, "text/html", httpMessage);
}

void Web::_handleSaveConfig()
{ // http://ESP01/saveConfig
  if (!_authenticated())
  {
    return;
  }

  debug.printLn(String(F("HTTP: Sending /saveConfig page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = "";
#ifdef ESP_32
  httpMessage = FPSTR(WM_HTTP_HEAD_START);
#elif defined(ESP_8266)
  httpMessage = FPSTR(HTTP_HEADER);
#endif
  httpMessage.replace("{v}", String(config.getNodeName()));
#ifdef ESP_32
  httpMessage += FPSTR(WM_HTTP_SCRIPT);
  httpMessage += FPSTR(WM_HTTP_STYLE);
#elif defined(ESP_8266)
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
#endif
  httpMessage += String(_style);

  bool shouldSaveWifi = false;
  // Check required values
  if (webServer.arg("wifiSSID") != "" && webServer.arg("wifiSSID") != String(WiFi.SSID()))
  { // Handle WiFi update
    config.setSaveNeeded();
    shouldSaveWifi = true;
    webServer.arg("wifiSSID").toCharArray(config.getWIFISSID(), 32);
    if (webServer.arg("wifiPass") != String("********"))
    {
      webServer.arg("wifiPass").toCharArray(config.getWIFIPass(), 64);
    }
  }
  if (webServer.arg("mqttServer") != "" && webServer.arg("mqttServer") != String(config.getMQTTServer()))
  { // Handle mqttServer
    config.setSaveNeeded();
    webServer.arg("mqttServer").toCharArray(config.getMQTTServer(), 64);
  }
  if (webServer.arg("mqttPort") != "" && webServer.arg("mqttPort") != String(config.getMQTTPort()))
  { // Handle mqttPort
    config.setSaveNeeded();
    webServer.arg("mqttPort").toCharArray(config.getMQTTPort(), 6);
  }
  if (webServer.arg("nodeName") != "" && webServer.arg("nodeName") != String(config.getNodeName()))
  { // Handle nodeName
    config.setSaveNeeded();
    String lowerNodeName = webServer.arg("nodeName");
    lowerNodeName.toLowerCase();
    lowerNodeName.toCharArray(config.getNodeName(), 16);
  }
  if (webServer.arg("groupName") != "" && webServer.arg("groupName") != String(config.getGroupName()))
  { // Handle groupName
    config.setSaveNeeded();
    webServer.arg("groupName").toCharArray(config.getGroupName(), 16);
  }
  // Check optional values
  if (webServer.arg("mqttUser") != String(config.getMQTTUser()))
  { // Handle mqttUser
    config.setSaveNeeded();
    webServer.arg("mqttUser").toCharArray(config.getMQTTUser(), 32);
  }
  if (webServer.arg("mqttPassword") != String("********"))
  { // Handle mqttPassword
    config.setSaveNeeded();
    webServer.arg("mqttPassword").toCharArray(config.getMQTTPassword(), 32);
  }
  if (webServer.arg("configUser") != String(_configUser))
  { // Handle configUser
    config.setSaveNeeded();
    webServer.arg("configUser").toCharArray(_configUser, 32);
  }
  if (webServer.arg("configPassword") != String("********"))
  { // Handle configPassword
    config.setSaveNeeded();
    webServer.arg("configPassword").toCharArray(_configPassword, 32);
  }
  if ((webServer.arg("debugTelnetEnabled") == String("on")) && !debug.getTelnetEnabled())
  { // debugTelnetEnabled was disabled but should now be enabled
    config.setSaveNeeded();
    debug.enableTelnet(true);
  }
  else if ((webServer.arg("debugTelnetEnabled") == String("")) && debug.getTelnetEnabled())
  { // debugTelnetEnabled was enabled but should now be disabled
    config.setSaveNeeded();
    debug.enableTelnet(false);
  }
  if ((webServer.arg("mdnsEnabled") == String("on")) && !config.getMDNSEnabled())
  { // mdnsEnabled was disabled but should now be enabled
    config.setSaveNeeded();
    config.setMDSNEnabled(true);
  }
  else if ((webServer.arg("mdnsEnabled") == String("")) && config.getMDNSEnabled())
  { // mdnsEnabled was enabled but should now be disabled
    config.setSaveNeeded();
    config.setMDSNEnabled(false);
  }

  if (config.getSaveNeeded())
  { // Config updated, notify user and trigger write to SPIFFS
    httpMessage += String(F("<meta http-equiv='refresh' content='15;url=/' />"));
#ifdef ESP_32
    httpMessage += FPSTR(WM_HTTP_HEAD_END);
#elif defined(ESP_8266)
    httpMessage += FPSTR(HTTP_HEADER_END);
#endif
    httpMessage += String(F("<h1>")) + String(config.getNodeName()) + String(F("</h1>"));
    httpMessage += String(F("<br/>Saving updated configuration values and restarting device"));
#ifdef ESP_32
    httpMessage += FPSTR(WM_HTTP_END);
#elif defined(ESP_8266)
    httpMessage += FPSTR(HTTP_END);
#endif
    webServer.send(200, "text/html", httpMessage);

    config.saveFile();
    if (shouldSaveWifi)
    {
      debug.printLn(String(F("CONFIG: Attempting connection to SSID: ")) + webServer.arg("wifiSSID"));
      esp.wiFiSetup();
    }
    esp.reset();
  }
  else
  { // No change found, notify user and link back to config page
    httpMessage += String(F("<meta http-equiv='refresh' content='3;url=/' />"));
#ifdef ESP_32
    httpMessage += FPSTR(WM_HTTP_HEAD_END);
#elif defined(ESP_8266)
    httpMessage += FPSTR(HTTP_HEADER_END);
#endif
    httpMessage += String(F("<h1>")) + String(config.getNodeName()) + String(F("</h1>"));
    httpMessage += String(F("<br/>No changes found, returning to <a href='/'>home page</a>"));
#ifdef ESP_32
    httpMessage += FPSTR(WM_HTTP_END);
#elif defined(ESP_8266)
    httpMessage += FPSTR(HTTP_END);
#endif
    webServer.send(200, "text/html", httpMessage);
  }
}

void Web::_handleResetConfig()
{ // http://ESP01/resetConfig
  if (!_authenticated())
  {
    return;
  }

  debug.printLn(String(F("HTTP: Sending /resetConfig page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = "";
#ifdef ESP_32
  httpMessage = FPSTR(WM_HTTP_HEAD_START);
#elif defined(ESP_8266)
  httpMessage = FPSTR(HTTP_HEADER);
#endif
  httpMessage.replace("{v}", String(config.getNodeName()));
#ifdef ESP_32
  httpMessage += FPSTR(WM_HTTP_SCRIPT);
  httpMessage += FPSTR(WM_HTTP_STYLE);
#elif defined(ESP_8266)
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
#endif
  httpMessage += String(_style);
#ifdef ESP_32
  httpMessage += FPSTR(WM_HTTP_HEAD_END);
#elif defined(ESP_8266)
  httpMessage += FPSTR(HTTP_HEADER_END);
#endif

  if (webServer.arg("confirm") == "yes")
  { // User has confirmed, so reset everything
    httpMessage += String(F("<h1>"));
    httpMessage += String(config.getNodeName());
    httpMessage += String(F("</h1><b>Resetting all saved settings and restarting device into WiFi AP mode</b>"));
#ifdef ESP_32
    httpMessage += FPSTR(WM_HTTP_END);
#elif defined(ESP_8266)
    httpMessage += FPSTR(HTTP_END);
#endif
    webServer.send(200, "text/html", httpMessage);
    delay(1000);
    config.clearFileSystem();
  }
  else
  {
    httpMessage += String(F("<h1>Warning</h1><b>This process will reset all settings to the default values and restart the device.  You may need to connect to the WiFi AP displayed on the panel to re-configure the device before accessing it again."));
    httpMessage += String(F("<br/><hr><br/><form method='get' action='resetConfig'>"));
    httpMessage += String(F("<br/><br/><button type='submit' name='confirm' value='yes'>reset all settings</button></form>"));
    httpMessage += String(F("<br/><hr><br/><form method='get' action='/'>"));
    httpMessage += String(F("<button type='submit'>return home</button></form>"));
#ifdef ESP_32
    httpMessage += FPSTR(WM_HTTP_END);
#elif defined(ESP_8266)
    httpMessage += FPSTR(HTTP_END);
#endif
    webServer.send(200, "text/html", httpMessage);
  }
}

void Web::_handleReboot()
{ // http://ESP01/reboot
  if (!_authenticated())
  {
    return;
  }

  debug.printLn(String(F("HTTP: Sending /reboot page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = "";
#ifdef ESP_32
  httpMessage = FPSTR(WM_HTTP_HEAD_START);
#elif defined(ESP_8266)
  httpMessage = FPSTR(HTTP_HEADER);
#endif
  httpMessage.replace("{v}", (String(config.getNodeName()) + " ESP reboot"));
#ifdef ESP_32
  httpMessage += FPSTR(WM_HTTP_SCRIPT);
  httpMessage += FPSTR(WM_HTTP_STYLE);
#elif defined(ESP_8266)
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
#endif
  httpMessage += String(_style);
  httpMessage += String(F("<meta http-equiv='refresh' content='10;url=/' />"));
#ifdef ESP_32
  httpMessage += FPSTR(WM_HTTP_HEAD_END);
#elif defined(ESP_8266)
  httpMessage += FPSTR(HTTP_HEADER_END);
#endif
  httpMessage += String(F("<h1>")) + String(config.getNodeName()) + String(F("</h1>"));
  httpMessage += String(F("<br/>Rebooting device"));
#ifdef ESP_32
  httpMessage += FPSTR(WM_HTTP_END);
#elif defined(ESP_8266)
  httpMessage += FPSTR(HTTP_END);
#endif
  webServer.send(200, "text/html", httpMessage);
  debug.printLn(F("RESET: Rebooting device"));
  esp.reset();
}
