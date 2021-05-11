#pragma once

#include "settings.h"
#include <Arduino.h>

// Additional CSS style
static const char _style[] = "<style>button{background-color:#03A9F4;}body{width:60%;margin:auto;}input:invalid{border:1px solid red;}input[type=checkbox]{width:20px;}</style>";

class Web
{
#pragma region Private

private:
#pragma endregion Private

#pragma region Public

public:
    // constructor
    Web(void) { _alive = false; }

    // destructor
    ~Web(void) { _alive = false; }

    void begin();
    void loop();

    const char *getStyle(void) { return _style; }

    char *getUser(void) { return _configUser; }
    void setUser(const char *value)
    {
        strncpy(_configUser, value, 32);
        _configUser[31] = '\0';
    }

    char *getPassword(void) { return _configPassword; }
    void setPassword(const char *value)
    {
        strncpy(_configPassword, value, 32);
        _configPassword[31] = '\0';
    }

    void resetWifiManager(void);

    void _handleNotFound();
    void _handleRoot();
    void _handleSaveConfig();
    void _handleResetConfig();
    void _handleReboot();
    void telnetPrintLn(bool enabled, String message);
    void telnetPrint(bool enabled, String message);

#pragma endregion Public

#pragma region Protected

protected:
    bool _alive;
    char _configUser[32];     // these two might belong in WebClass
    char _configPassword[32]; // these two might belong in WebClass
    uint32_t _tftFileSize;

    bool _authenticated(void);
    void _handleTelnetClient();
    void _setupHTTP();
    void _setupMDNS();
    void _setupTelnet();

#pragma endregion Protected
};