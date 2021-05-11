#pragma once

#include "settings.h"
#include <Arduino.h>

enum source_t
{
    MQTT,
    SYSTEM,
    WIFI
};

class Debug
{
#pragma region Private

private:
#pragma endregion Private

#pragma region Public

public:
    // constructor
    Debug(void) { _alive = false; }

    // destructor
    ~Debug(void) { _alive = false; }

    // called on setup to initialise all our things
    void begin(void)
    {
        _verboseDebugMQTT = DEBUG_MQTT_VERBOSE;
        _verboseDebugSystem = true;
        _verboseDebugWiFi = true;
        _telnetEnabled = DEBUG_TELNET_ENABLED;
        _alive = true;
    }

    inline void disableTelnet(bool enable = false) { _telnetEnabled = enable; }
    inline void enableTelnet(bool enable = true) { _telnetEnabled = enable; }
    inline bool getTelnetEnabled() { return _telnetEnabled; }

    void print(String debugText);
    void printLn(String debugText);

    inline void printLn(enum source_t source, String debugText)
    { // a wrapper version that might print or might not
        if (source == MQTT && _verboseDebugMQTT)
        {
            printLn(debugText);
        }
        if (source == WIFI && _verboseDebugWiFi)
        {
            printLn(debugText);
        }
        if (source == SYSTEM && _verboseDebugSystem)
        {
            printLn(debugText);
        }
    }

    // uniquely named wrapper versions
    inline void printLnMQTT(String debugText)
    {
        if (_verboseDebugMQTT)
        {
            printLn(debugText);
        }
    }
    inline void printLnSystem(String debugText)
    {
        if (_verboseDebugSystem)
        {
            printLn(debugText);
        }
    }
    inline void printLnWiFi(String debugText)
    {
        if (_verboseDebugWiFi)
        {
            printLn(debugText);
        }
    }

    inline void verbosity(enum source_t source, bool verbose = false)
    { // enable or disable printing blocks
        if (source == MQTT)
        {
            _verboseDebugMQTT = verbose;
        }
        if (source == WIFI)
        {
            _verboseDebugWiFi = verbose;
        }
        if (source == SYSTEM)
        {
            _verboseDebugSystem = verbose;
        }
    }

#pragma endregion Public

#pragma region Protected

protected:
    bool _alive;              // Flag that data structures are initialised and functions can run without error
    bool _telnetEnabled;      // Enable telnet debug output
    bool _verboseDebugMQTT;   // set false to have fewer printf from MQTT
    bool _verboseDebugSystem; // set false to have fewer printf from System
    bool _verboseDebugWiFi;   // set false to have fewer printf from WiFi

#pragma endregion Protected
};