#ifndef WEBSERVERR_H
#define WEBSERVERR_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <ReadFile/readfile.h>

extern JsonDocument wifiJson, userJson, envJson;
extern String wifiConf, userConf, envConf;

void setupWebserver(AsyncWebServer &server);

#endif