#pragma once

// if the flag "COMMON_IS_LOCAL" is defined, then these classes are Actual, not Extern
// note that if two files declare COMMON_IS_LOCAL, you will get linker errors about duplicated symbols.
#ifdef COMMON_IS_LOCAL
#define COMMON_EXTERN
#else
#define COMMON_EXTERN extern
#endif

#ifdef ESP_32
#include <rom/rtc.h>
// #elif defined(ESP_8266)
// ESP8266
#endif

#include "settings.h"
#include <Arduino.h>

#include "config.h"
COMMON_EXTERN Config config;

#include "debug.h"
COMMON_EXTERN Debug debug;

#include "esp.h"
COMMON_EXTERN Esp esp;  // our ESP8266 Micro/SoC

#include "mqttSvc.h"
COMMON_EXTERN MqttSvc mqtt;  // our MQTT Object

#include "web.h"
COMMON_EXTERN Web web;  // our HTTP Server Object
