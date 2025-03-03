#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <DFRobot_PH.h>
#include <UserConfig/UserConfig.h>
#include <WifiConfig/WifiConfig.h>
#include <WiFi.h>
#include <time.h>
#include <ReadFile/readfile.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define ESPADC 4095.0  // the esp Analog Digital Convertion value
#define ESPVOLTAGE 3.3 // the esp voltage supply value

#define BOOT_BUTTON 0

// JSON global
JsonDocument wifiJson;
String wifiConf;
JsonDocument userJson;
String userConf;
JsonDocument envJson;
String envConf;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Webserver
AsyncWebServer server(80);

// LCD Setup
LiquidCrystal_I2C lcd(0x27, 16, 2);
int Menu = 1;

// Temperature setup
#define TEMPERATURE_PIN 4
OneWire oneWire(TEMPERATURE_PIN);
DallasTemperature sensors(&oneWire);

// PH setup
#define PH_PIN 35
DFRobot_PH ph;

// Turbidity setup
#define TURBIDITY_PIN 34

// Access Point
const String ssidAP = "Aqua Watch";
const String passwordAP = "aquawatch";

// global variables
float phValue, phVoltage, temperature, turbidity;

// put function declarations here:
void setupLCD();
float getTemperature();
float getVoltage(uint8_t);
float getPh();
float getTurbidity();
void printDigits(int);
void LCDPrint(const String &text, const int duration);
void printMenu();
void setupWebserver();
void connectWifi();

// Setup Function
void setup()
{
  Serial.begin(115200);
  EEPROM.begin(32);
  readFileInit();
  setupLCD();
  sensors.begin();
  ph.begin();

  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  LCDPrint("Setting AP...", 2);
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();
  WiFi.softAP(ssidAP, passwordAP);

  LCDPrint("Scanning networks...", 2);
  WiFi.scanNetworks(true);

  setupWebserver();
}

// Loop function
void loop()
{
  static unsigned long timepoint = millis();
  static unsigned long pressStartTime = 0;
  static bool actionTriggered = false;

  if (digitalRead(BOOT_BUTTON) == LOW)
  { // Button is pressed
    if (pressStartTime == 0)
    {
      pressStartTime = millis(); // Start timing
    }

    if (!actionTriggered && (millis() - pressStartTime >= 1000U))
    {
      Menu++;
      if (Menu > 4)
        Menu = 1;
      actionTriggered = true; // Ensure action triggers only once
    }
  }
  else
  {
    // Button released → Reset tracking variables
    pressStartTime = 0;
    actionTriggered = false;
  }

  if (millis() - timepoint > 1000U)
  {
    timepoint = millis();

    temperature = getTemperature();
    phValue = getPh();
    turbidity = getTurbidity();

    printMenu();

    if (WiFi.status() == WL_CONNECTED)
    {
      if (!timeClient.isTimeSet())
      {
        timeClient.begin();
        timeClient.setTimeOffset(3600 * 3);
        timeClient.update();
      }
    }
    else
      connectWifi();
  }
  ph.calibration(phVoltage, temperature);
}

void setupLCD()
{
  lcd.init();
  lcd.backlight();
}

float getVoltage(uint8_t pin)
{
  return analogRead(pin) * (ESPVOLTAGE / ESPADC);
}

float getTemperature()
{
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}

float getPh()
{
  int val = getVoltage(PH_PIN);
  return ph.readPH(val, temperature);
}

float getTurbidity()
{
  int val = analogRead(TURBIDITY_PIN);
  return map(val, 0, 2450, 0, 100);
}

void printDigits(int digits)
{
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

void LCDPrint(const String &text, const int duration)
{
  lcd.clear();

  lcd.setCursor(0, 0);
  if (text.length() <= 16)
  {
    lcd.print(text);
  }
  else
  {
    lcd.print(text.substring(0, 16));
    lcd.setCursor(0, 1);
    lcd.print(text.substring(16));
  }
  delay(duration * 1000);
  lcd.clear();
}

void printMenu()
{
  lcd.clear();
  lcd.home();
  switch (Menu)
  {
  case 1:
    lcd.printf("%4.2fC", temperature);
    lcd.setCursor(11, 0);
    lcd.printf("%4.0f%%", turbidity);
    lcd.setCursor(0, 1);
    lcd.print(int(phValue));
    lcd.print("pH");
    if (timeClient.isTimeSet())
    {
      lcd.setCursor(8, 1);
      lcd.print(timeClient.getFormattedTime());
    }

    break;

  case 2:
  {
    unsigned long milliseconds = millis();
    unsigned long seconds = milliseconds / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;

    lcd.print("Uptime:");
    lcd.setCursor(0, 1);
    lcd.printf("%lu:%lu:%lu:%lu", days, hours % 24, minutes % 60, seconds % 60);
    break;
  }

  case 3:
    lcd.print("Access Point:");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.softAPIP());
    break;

  case 4:
    if (WiFi.status() == WL_CONNECTED)
    {
      if (wifiJson.isNull())
      {
        wifiConf = readFileToString("/wifi.json");
        deserializeJson(wifiJson, wifiConf);
      }

      if (wifiJson["ssid"].is<String>())
      {
        lcd.setCursor(0, 0);
        lcd.print(String(wifiJson["ssid"].as<String>()));
        lcd.setCursor(0, 1);
        lcd.print(WiFi.localIP().toString());
      }
    }
    else
    {
      lcd.print("Network not connected.");
    }
    break;
  }
}

void connectWifi()
{
  wifiConf = readFileToString("/wifi.json");
  if (!wifiConf.isEmpty())
  {
    deserializeJson(wifiJson, wifiConf);

    if (wifiJson["ssid"].is<String>() && wifiJson["password"].is<String>())
    {
      WiFi.begin(wifiJson["ssid"].as<String>(), wifiJson["password"].as<String>());

      int retryCount = 0;
      while (WiFi.status() != WL_CONNECTED && retryCount < 10)
      {
        retryCount++;
        LCDPrint("Connecting... " + String(retryCount), 1);
      }

      if (WiFi.status() == WL_CONNECTED)
        LCDPrint("Connected!", 2);
      else
        LCDPrint("Failed to connect.", 2);
    }
  }
}

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

  if (!jsonObj["id"] || !jsonObj["name"])
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
    resJson["data"] = NULL;

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

  serializeJson(resJson, res);
  request->send(200, "application/json", res);
  resJson.clear();
  envJson.clear();
  res.clear();

  return;
}

void setupWebserver()
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

  wifiConf = readFileToString("/wifi.json");

  if (!wifiConf.isEmpty())
  {
    DeserializationError error = deserializeJson(wifiJson, wifiConf);

    if (error)
    {
      SPIFFS.remove("/wifi.json");
      return;
    }

    WiFi.begin(wifiJson["ssid"].as<String>(), wifiJson["password"].as<String>());

    wifiJson.clear();

    LCDPrint("Connecting...", 2);
    int retryCount = 0;
    while (WiFi.status() != WL_CONNECTED && retryCount < 10)
    {
      retryCount++;
      LCDPrint("Connecting... " + String(retryCount), 1);
    }

    if (WiFi.status() == WL_CONNECTED)
      LCDPrint("Connected!", 2);
    else
      LCDPrint("Failed to connect.", 2);
  }
}