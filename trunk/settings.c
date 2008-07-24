#include "settings.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>

int read_config (engine_config_t *config)
{
    HKEY key;
    DWORD dw, size; 
    
    engine_get_default_config(config);

    if(RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE\\Minicast"), 0, KEY_READ, &key)
        == ERROR_SUCCESS)
    {
        size = sizeof(config->network.stream_name); RegQueryValueEx( key, "Stream Name",
            NULL, NULL, config->network.stream_name, &size );
        config->network.stream_name[sizeof(config->network.stream_name)-1] = '\0';    
        RegCloseKey(key);
    }
    
    if(RegOpenKeyEx( HKEY_CURRENT_USER, TEXT("SOFTWARE\\Minicast\\Encoder"), 0, KEY_READ, &key)
        == ERROR_SUCCESS)
    {
        size = sizeof(dw); if(RegQueryValueEx(key, "Bitrate",  NULL, NULL, &dw, &size)
            == ERROR_SUCCESS) config->encoder.bitrate  = (short)dw;
        size = sizeof(dw); if(RegQueryValueEx(key, "Channels", NULL, NULL, &dw, &size)
            == ERROR_SUCCESS) config->encoder.channels = (short)dw;
        RegCloseKey(key);
    }
    
    if(RegOpenKeyEx( HKEY_CURRENT_USER, TEXT("SOFTWARE\\Minicast\\Network"), 0, KEY_READ, &key )
        == ERROR_SUCCESS)
    {
        size = sizeof(dw); if(RegQueryValueEx(key, "Address",  NULL, NULL, &dw, &size)
            == ERROR_SUCCESS) config->network.address          = (unsigned long)dw;
        size = sizeof(dw); if(RegQueryValueEx(key, "Port",     NULL, NULL, &dw, &size)
            == ERROR_SUCCESS) config->network.port             = (unsigned short)dw;
        size = sizeof(dw); if(RegQueryValueEx(key, "Connection Limit", NULL, NULL, &dw, &size)
            == ERROR_SUCCESS) config->network.connection_limit = (unsigned short)dw;
        RegCloseKey(key);
    }
        
    return 0;
}

int write_config(engine_config_t *config)
{
    HKEY key;
    DWORD dw; 

    if(RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE\\Minicast"), 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS)
    {
        RegSetValueEx( key, "Stream Name", 0, REG_SZ,
            config->network.stream_name, strlen(config->network.stream_name) + 1 );
        RegCloseKey(key);
    }

    if(RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE\\Minicast\\Encoder"), 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS)
    {
        dw = config->encoder.bitrate;  RegSetValueEx( key, "Bitrate",  0, REG_DWORD, &dw, sizeof(dw) );
        dw = config->encoder.channels; RegSetValueEx( key, "Channels", 0, REG_DWORD, &dw, sizeof(dw) );
        RegCloseKey(key);
    }
    
    if(RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE\\Minicast\\Network"), 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS)
    {   
        dw = config->network.address; RegSetValueEx( key, "Address", 0, REG_DWORD, &dw, sizeof(dw) );
        dw = config->network.port;    RegSetValueEx( key, "Port",    0, REG_DWORD, &dw, sizeof(dw) );
        dw = config->network.connection_limit; RegSetValueEx(
            key, "Connection Limit", 0, REG_DWORD, &dw, sizeof(dw) );
        RegCloseKey(key);
    }
    
    return 0;
}
