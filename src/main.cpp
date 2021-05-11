// set a flag before calling common.h to announce that all the class are local, not extern, here.
#define COMMON_IS_LOCAL

#include "common.h"

#include <ArduinoOTA.h>

void setup()
{
  Serial.begin(9600); // Serial - LCD RX (after swap), debug TX

  debug.begin();

  debug.printLn(SYSTEM, String(F("SYSTEM: Starting v")) + String(VERSION));
#ifdef ESP_32
  debug.printLn(SYSTEM, String(F("SYSTEM: Last reset reason: ")) + String((int)rtc_get_reset_reason(0)));
  debug.printLn(SYSTEM, String(F("SYSTEM: ESP SDK version: ")) + String(ESP.getSdkVersion()));
  debug.printLn(SYSTEM, String(F("SYSTEM: Heap Status: ")) + String(ESP.getFreeHeap()));
#elif defined(ESP_8266)
  debug.printLn(SYSTEM, String(F("SYSTEM: Last reset reason: ")) + String(ESP.getResetInfo()));
  debug.printLn(SYSTEM, String(F("SYSTEM: espCore: ")) + String(ESP.getCoreVersion()));
  debug.printLn(SYSTEM, String(F("SYSTEM: Heap Status: ")) + String(ESP.getFreeHeap()) + String(F(" ")) + String(ESP.getHeapFragmentation()) + String(F("%")));
#endif

  config.begin();
  esp.begin();

#ifdef ESP_32
  debug.printLn(SYSTEM, String(F("SYSTEM: Heap Status: ")) + String(ESP.getFreeHeap()));
#elif defined(ESP_8266)
  debug.printLn(SYSTEM, String(F("SYSTEM: Heap Status: ")) + String(ESP.getFreeHeap()) + String(F(" ")) + String(ESP.getHeapFragmentation()) + String(F("%")));
#endif

  web.begin();
  mqtt.begin();

  debug.printLn(SYSTEM, F("SYSTEM: System init complete."));
}

void loop()
{
  esp.loop();
  mqtt.loop();
  ArduinoOTA.handle(); // Arduino OTA loop
  web.loop();
}