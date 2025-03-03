#ifndef WIFICONFIG_H
#define WIFICONFIG_H

#include <Arduino.h>
#include <WiFi.h>

class WifiConfig
{
public:
    String ssid;
    String password;
    IPAddress IPAddressAP;

public:
    // Constructor
    WifiConfig();
};

#endif
