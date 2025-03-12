
#include "Webserverr.h"

JsonDocument wifiJson, userJson, envJson;
String wifiConf, userConf, envConf;

// Server handler: Return WiFi configuration
void handleGetWifiConfig(AsyncWebServerRequest *request)
{
    JsonDocument resJson;
    String res;

    wifiConf = readFileToString("/wifi.json");

    if (wifiConf.isEmpty())
    {
        resJson["message"] = "No WiFi configuration saved.";
    }
    else
    {
        DeserializationError error = deserializeJson(wifiJson, wifiConf);

        if (error)
        {
            SPIFFS.remove("/wifi.json");
            request->send(400, "application/json", "{\"message\":\"Failed to read WiFi configuration.\"}");
            return;
        }

        resJson["message"] = "Wifi conf. fetched successfully";
        resJson["data"]["ssid"] = wifiJson["ssid"].as<String>();
        resJson["data"]["password"] = wifiJson["password"].as<String>();
    }

    serializeJson(resJson, res);
    request->send(200, "application/json", res);
    return;
}

// Server handler: Save WiFi configuration
void handleSaveWifiConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    JsonDocument json;
    deserializeJson(json, data, len);
    JsonObject jsonObj = json.as<JsonObject>();

    if (!jsonObj["ssid"] || !jsonObj["password"])
    {
        request->send(400, "application/json", "{\"message\":\"SSID and Password are required.\"}");
        return;
    }

    File file = SPIFFS.open("/wifi.json", FILE_WRITE);
    if (!file)
    {
        request->send(500, "application/json", "{\"message\":\"Failed to save WiFi configuration.\"}");
        return;
    }

    serializeJson(jsonObj, file);
    file.close();

    request->send(200, "application/json", "{\"message\":\"WiFi configuration has been saved.\"}");
    jsonObj.clear();
    json.clear();
}

// Server handler: Scan for available WiFi networks
void handleWifiScan(AsyncWebServerRequest *request)
{
    String json = "[";
    int n = WiFi.scanComplete();

    if (n == -2)
    {
        WiFi.scanNetworks(true);
    }
    else if (n > 0)
    {
        for (int i = 0; i < n; ++i)
        {
            if (i > 0)
                json += ",";
            json += "{";
            json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
            json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
            json += "\"id\":" + String(i + 1) + ",";
            json += "\"isOpen\":\"" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "closed") + "\"";
            json += "}";
        }
        WiFi.scanDelete();
    }

    json += "]";
    request->send(200, "application/json", json);
}

// Server handler: Return wifi status
void handleWifiStatus(AsyncWebServerRequest *request)
{
    JsonDocument resJson;
    String res;
    // WL_IDLE_STATUS = 0, WL_NO_SSID_AVAIL = 1, WL_SCAN_COMPLETED = 2, WL_CONNECTED = 3, WL_CONNECT_FAILED = 4, WL_CONNECTION_LOST = 5,
    // WL_DISCONNECTED = 6

    wl_status_t status = WiFi.status();

    switch (status)
    {
    case WL_IDLE_STATUS:
        resJson["status"] = "Iddle";
        break;

    case WL_NO_SSID_AVAIL:
        resJson["status"] = "Wifi not found";
        break;

    case WL_SCAN_COMPLETED:
        resJson["status"] = "Scan completed";
        break;

    case WL_CONNECT_FAILED:
        resJson["status"] = "Failed to connect";
        break;

    case WL_CONNECTION_LOST:
        resJson["status"] = "Connection lost";
        break;

    case WL_DISCONNECTED:
        resJson["status"] = "Disconnected";
        break;

    case WL_CONNECTED:
        resJson["status"] = "Connected";
        break;
    }

    // Get wifi config
    wifiConf = readFileToString("/wifi.json");

    if (wifiConf.isEmpty())
    {
        resJson["data"].clear();
        serializeJson(resJson, res);
        request->send(200, "application/json", res);
    }
    else
    {
        DeserializationError error = deserializeJson(wifiJson, wifiConf);

        if (error)
        {
            SPIFFS.remove("/wifi.json");

            resJson["data"].clear();
            serializeJson(resJson, res);
            request->send(200, "application/json", res);
        }

        resJson["data"]["ssid"] = wifiJson["ssid"].as<String>();
        resJson["data"]["password"] = wifiJson["password"].as<String>();
        resJson["data"]["ip"] = WiFi.localIP().toString();

        serializeJson(resJson, res);
        request->send(200, "application/json", res);
    }
}

// Server handler: Restart the device
void handleRestart(AsyncWebServerRequest *request)
{
    request->send(200, "application/json", "{\"message\":\"Restarting...\"}");
    ESP.restart();
}

// Server handler: Connect to a network
void handleConnect(AsyncWebServerRequest *request)
{
    JsonDocument resJson;
    String res;
    wifiConf = readFileToString("/wifi.json");

    if (wifiConf.isEmpty())
    {
        resJson["message"] = "No WiFi configuration saved.";
        serializeJson(resJson, res);
        request->send(400, "application/json", res);
        resJson.clear();
        res.clear();
        return;
    }

    DeserializationError error = deserializeJson(wifiJson, wifiConf);

    if (error)
    {
        SPIFFS.remove("/wifi.json");
        request->send(400, "application/json", "{\"message\":\"Failed to read WiFi configuration.\"}");
        return;
    }

    request->send(200, "application/json", "{\"message\":\"Connecting...\"}");

    WiFi.begin(wifiJson["ssid"].as<String>(), wifiJson["password"].as<String>());

    wifiJson.clear();
}

// Server handler: Save user conf
void handleSaveUserConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    JsonDocument json;
    deserializeJson(json, data, len);
    JsonObject jsonObj = json.as<JsonObject>();

    if (!jsonObj["email"] || !jsonObj["password"])
    {
        request->send(400, "application/json", "{\"message\":\"Email and Password are required.\"}");
        return;
    }

    File file = SPIFFS.open("/user.json", FILE_WRITE);
    if (!file)
    {
        request->send(500, "application/json", "{\"message\":\"Failed to save user configuration.\"}");
        return;
    }

    serializeJson(jsonObj, file);
    file.close();

    request->send(200, "application/json", "{\"message\":\"User configuration has been saved.\"}");
    jsonObj.clear();
    json.clear();
}

// Server handler: Return User configuration
void handleGetUserConfig(AsyncWebServerRequest *request)
{
    JsonDocument resJson;
    String res;
    userConf = readFileToString("/user.json");

    if (userConf.isEmpty())
    {
        resJson["message"] = "No user configuration saved.";
        resJson["data"] = NULL;

        serializeJson(resJson, res);
        request->send(400, "application/json", res);
        resJson.clear();
        res.clear();
        return;
    }

    DeserializationError error = deserializeJson(userJson, userConf);

    if (error)
    {
        SPIFFS.remove("/user.json");
        request->send(400, "application/json", "{\"message\":\"Failed to read user configuration.\"}");
        return;
    }

    resJson["message"] = "User conf. fetched successfully";
    resJson["data"]["email"] = userJson["email"].as<String>();
    resJson["data"]["password"] = userJson["password"].as<String>();

    serializeJson(resJson, res);
    request->send(200, "application/json", res);
    resJson.clear();
    res.clear();

    return;
}

// Server handler: Save environment
void handleSaveEnvironment(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    JsonDocument json;
    deserializeJson(json, data, len);
    JsonObject jsonObj = json.as<JsonObject>();

    if (jsonObj["id"].isNull() || jsonObj["name"].isNull() || jsonObj["enable_monitoring"].isNull())
    {
        request->send(400, "application/json", "{\"message\":\"ID and Name are required.\"}");
        json.clear();
        jsonObj.clear();
        return;
    }

    File file = SPIFFS.open("/environment.json", FILE_WRITE);
    if (!file)
    {
        request->send(500, "application/json", "{\"message\":\"Failed to save environment.\"}");
        json.clear();
        jsonObj.clear();
        return;
    }

    serializeJson(jsonObj, file);
    file.close();

    request->send(200, "application/json", "{\"message\":\"Environment has been saved.\"}");
    jsonObj.clear();
    json.clear();
}

// Server handler: Return environment
void handleGetEnvironment(AsyncWebServerRequest *request)
{
    JsonDocument resJson;
    String res;
    envConf = readFileToString("/environment.json");

    if (envConf.isEmpty())
    {
        resJson["message"] = "No environment saved.";
        resJson["data"].clear();

        serializeJson(resJson, res);
        request->send(400, "application/json", res);
        resJson.clear();
        res.clear();
        return;
    }

    DeserializationError error = deserializeJson(envJson, envConf);

    if (error)
    {
        SPIFFS.remove("/environment.json");
        request->send(400, "application/json", "{\"message\":\"Failed to read environment.\"}");
        return;
    }

    resJson["message"] = "Environment fetched successfully";
    resJson["data"]["id"] = envJson["id"].as<String>();
    resJson["data"]["name"] = envJson["name"].as<String>();
    resJson["data"]["enable_monitoring"] = envJson["enable_monitoring"].as<bool>();

    serializeJson(resJson, res);
    request->send(200, "application/json", res);
    resJson.clear();
    envJson.clear();
    res.clear();

    return;
}

void setupWebserver(AsyncWebServer &server)
{
    server.on("/api/wifi-conf", HTTP_GET, handleGetWifiConfig);
    server.on("/api/wifi-conf", HTTP_POST, [](AsyncWebServerRequest *request)
              { request->send(400, "application/json", "{\"message\":\"Body is required.\"}"); }, nullptr, handleSaveWifiConfig);
    server.on("/api/scan", HTTP_GET, handleWifiScan);
    server.on("/api/status", HTTP_GET, handleWifiStatus);
    server.on("/api/restart", HTTP_GET, handleRestart);
    server.on("/api/connect", HTTP_GET, handleConnect);
    server.on("/api/user-conf", HTTP_GET, handleGetUserConfig);
    server.on("/api/user-conf", HTTP_POST, [](AsyncWebServerRequest *request)
              { request->send(400, "application/json", "{\"message\":\"Body is required.\"}"); }, nullptr, handleSaveUserConfig);
    server.on("/api/environment", HTTP_GET, handleGetEnvironment);
    server.on("/api/environment", HTTP_POST, [](AsyncWebServerRequest *request)
              { request->send(400, "application/json", "{\"message\":\"Body is required.\"}"); }, nullptr, handleSaveEnvironment);

    // Serve static files from SPIFFS
    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

    // Catch-all route to handle React's single-page app routing
    server.onNotFound([](AsyncWebServerRequest *request)
                      { request->send(SPIFFS, "/index.html", String(), false); });

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

    server.begin();
}