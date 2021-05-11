#pragma once

// OPTIONAL: Assign default values here.
#define DEFAULT_WIFI_SSID ("") // Leave unset for wireless autoconfig. Note that these values will be lost
#define DEFAULT_WIFI_PASS ("") // when updating, but that's probably OK because they will be saved in EEPROM.

// These defaults may be overwritten with values saved by the web interface
// Note that MQTT prefers dotted quad address, but MQTTS prefers fully qualified domain names (fqdn)
// Note that MQTTS works best using NTP to obtain Time
#define DEFAULT_MQTT_SERVER ("")
#define DEFAULT_MQTT_PORT ("1883")
#define DEFAULT_MQTT_USER ("")
#define DEFAULT_MQTT_PASS ("")
#define DEFAULT_NODE_NAME ("ESP01")
#define DEFAULT_GROUP_NAME ("ESPs")
#define DEFAULT_CONFIG_USER ("admin")
#define DEFAULT_CONFIG_PASS ("")

#define ASECOND (1000)
#define AMINUTE (60 * ASECOND)
#define ANHOUR (3600 * ASECOND)

#define VERSION (0.01)                       // Current software release version
#define WIFI_CONFIG_PASSWORD ("esppassword") // First-time config WPA2 password
#define WIFI_CONFIG_AP ("ESPBase")           // First-time config WPA2 password
#define CONNECTION_TIMEOUT (300)             // Timeout for WiFi and MQTT connection attempts in seconds
#define RECONNECT_TIMEOUT (15)               // Timeout for WiFi reconnection attempts in seconds

// by default, on power on read config.json from the spiffs
#define DISABLE_CONFIG_READ (false) // if true, do not read config.json from spiffs

#define MQTT_MAX_PACKET_SIZE (4096)               // Size of buffer for incoming MQTT message
#define MQTT_STATUS_UPDATE_INTERVAL (5 * AMINUTE) // Time in msec between publishing MQTT status updates (5 minutes)

#define MDNS_ENABLED (true) // mDNS enabled

#define DEBUG_MQTT_VERBOSE (true)    // set false to have fewer printf from MQTT
#define DEBUG_TELNET_ENABLED (false) // Enable telnet debug output