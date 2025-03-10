#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <DFRobot_PH.h>
#include <WiFi.h>
#include <time.h>
#include <ReadFile/readfile.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <DissolvedOxygen/DissolvedOxygen.h>
#include <Webserverr/Webserverr.h>
#include <HTTPClient.h>

// Constants
#define ESPADC 4095.0
#define ESPVOLTAGE 3300
#define BOOT_BUTTON 0
#define TEMPERATURE_PIN 4
#define PH_PIN 34
#define TURBIDITY_PIN 32
#define DO_PIN 35
#define AP_SSID "Aqua Watch"
#define AP_PASSWORD "aquawatch"

// Global Variables
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
AsyncWebServer server(80);
LiquidCrystal_I2C lcd(0x27, 16, 2);
OneWire oneWire(TEMPERATURE_PIN);
DallasTemperature sensors(&oneWire);
DFRobot_PH ph;
float phValue, temperature, turbidity, dissolvedOxygen;
int Menu = 1;
bool syncEnable = false;

// User Configuration
const String API_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImV3bHVsaGtyZWZvYmFvb3htY3R0Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3MzM4NDM5ODIsImV4cCI6MjA0OTQxOTk4Mn0.LGGDDHaAcH4f645jT3IC5-adPSku4BbRip52-Ui6e08"; // Replace with your API key
const String refresh_session_url = "https://ewlulhkrefobaooxmctt.supabase.co/auth/v1/token?grant_type=refresh_token";
const String login_url = "https://ewlulhkrefobaooxmctt.supabase.co/auth/v1/token?grant_type=password";
const String insert_url = "https://ewlulhkrefobaooxmctt.supabase.co/rest/v1/dataset";
String access_token;
String refresh_token;

// Function Declarations
float getTemperature();
float getVoltage(uint8_t);
float getPh();
float getTurbidity();
void handleButtonPress();
void printMenu();
void LCDPrint(const String &, int);
void connectWifi();
bool signIn();
bool refreshSession();
bool sendData();
bool checkEnv();

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(32);
  readFileInit();
  lcd.init();
  lcd.backlight();
  sensors.begin();
  ph.begin();
  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  LCDPrint("Setting AP...", 2);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  WiFi.scanNetworks(true);

  setupWebserver(server);

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

void loop()
{
  static unsigned long timepoint = millis();
  static bool isSent = false;
  static int sentCount;
  handleButtonPress();

  if (millis() - timepoint > 1000U)
  {
    timepoint = millis();

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
    {
      connectWifi();
    }

    temperature = getTemperature();
    phValue = getPh();
    turbidity = getTurbidity();
    dissolvedOxygen = getDO(DO_PIN, int(temperature));

    printMenu();

    if (timeClient.getMinutes() % 5 == 0 && WiFi.status() == WL_CONNECTED)
    {
      if (!isSent && syncEnable)
      {
        isSent = true;
        bool isLogged = false;

        if (access_token.isEmpty())
          isLogged = signIn();
        else
          isLogged = refreshSession();

        if (!isLogged || !checkEnv())
        {
          return;
        }

        if (sendData())
          sentCount++;
      }
    }
    else
      isSent = false;
  }

  ph.calibration(getVoltage(PH_PIN), temperature);
}

void handleButtonPress()
{
  static unsigned long pressStartTime = 0;
  static bool actionTriggered = false;

  if (digitalRead(BOOT_BUTTON) == LOW)
  {
    if (pressStartTime == 0)
      pressStartTime = millis();

    if (!actionTriggered && (millis() - pressStartTime >= 1000U))
    {
      Menu = (Menu % 5) + 1;
      actionTriggered = true;
    }
  }
  else
  {
    pressStartTime = 0;
    actionTriggered = false;
  }
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
  return ph.readPH(getVoltage(PH_PIN), temperature);
}

float getTurbidity()
{
  return map(getVoltage(TURBIDITY_PIN), 0, 2080, 0, 100);
}

void LCDPrint(const String &text, int duration)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(text.length() <= 16 ? text : text.substring(0, 16));
  if (text.length() > 16)
  {
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
    lcd.setCursor(9, 0);
    lcd.print(String(phValue) + "pH");
    lcd.setCursor(0, 1);
    lcd.print(String(dissolvedOxygen) + "mg/L");
    lcd.setCursor(11, 1);
    lcd.printf("%4.0f%%", turbidity);
    break;
  case 2:
  {
    unsigned long sec = millis() / 1000;
    lcd.print("Uptime:");
    lcd.setCursor(0, 1);
    lcd.printf("%lu:%lu:%lu:%lu", sec / 86400, (sec / 3600) % 24, (sec / 60) % 60, sec % 60);
  }
  break;
  case 3:
    lcd.print("Time: ");
    lcd.setCursor(0, 1);
    if (timeClient.isTimeSet())
    {
      lcd.print(timeClient.getFormattedTime());
    }
    else
    {
      lcd.print("Time is not configured");
    }
    break;
  case 4:
    lcd.print("Access Point:");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.softAPIP());
    break;
  case 5:
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
        lcd.print(wifiJson["ssid"].as<String>());
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
  static int trying = 0;
  wifiConf = readFileToString("/wifi.json");

  if (trying > 10)
  {
    SPIFFS.remove("/wifi.json");
  }

  if (!wifiConf.isEmpty())
  {
    deserializeJson(wifiJson, wifiConf);
    if (wifiJson["ssid"].is<String>() && wifiJson["password"].is<String>())
    {
      WiFi.begin(wifiJson["ssid"].as<String>(), wifiJson["password"].as<String>());
      for (int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++)
      {
        LCDPrint("Connecting... " + String(i + 1), 1);
      }
      LCDPrint(WiFi.status() == WL_CONNECTED ? "Connected!" : "Failed to connect.", 2);
    }
    trying++;
  }
}

bool signIn()
{
  HTTPClient http;
  int httpResponseCode;

  http.begin(login_url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", API_KEY);

  userConf = readFileToString("/user.json");
  deserializeJson(userJson, userConf);

  String requestBody;
  serializeJson(userJson, requestBody);

  httpResponseCode = http.POST(requestBody);

  if (httpResponseCode != 200)
  {
    http.end();
    return false;
  }

  String payload = http.getString();
  JsonDocument response;
  deserializeJson(response, payload);
  access_token = response["access_token"].as<String>();
  refresh_token = response["refresh_token"].as<String>();

  http.end();
  return true;
}

bool refreshSession()
{
  HTTPClient http;
  int httpResponseCode;

  http.begin(refresh_session_url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", API_KEY);

  httpResponseCode = http.POST("{'refresh_token': '" + refresh_token + "'}");

  if (httpResponseCode != 200)
  {
    http.end();
    return false;
  }

  String payload = http.getString();
  JsonDocument response;
  deserializeJson(response, payload);
  access_token = response["access_token"].as<String>();
  refresh_token = response["refresh_token"].as<String>();

  http.end();
  return true;
}

bool sendData()
{
  HTTPClient http;
  int httpResponseCode;

  Serial.println("Sending data");
  if (!envJson["id"].is<String>())
  {
    LCDPrint("Env ID missing!", 2);
    return false;
  }

  JsonDocument payloadJson;
  String payloadString;

  payloadJson["env_id"] = envJson["id"].as<String>();
  payloadJson["temp"] = temperature;
  payloadJson["dissolved_oxygen"] = dissolvedOxygen;
  payloadJson["turbidity"] = turbidity / 100;
  payloadJson["ph"] = phValue;

  serializeJson(payloadJson, payloadString);
  http.begin(insert_url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", API_KEY);
  http.addHeader("Authorization", "Bearer " + access_token);

  httpResponseCode = http.POST(payloadString);

  if (httpResponseCode != 201)
  {
    LCDPrint("Failed to send!", 2);
    http.end();
    access_token = "";
    refresh_token = "";
    return false;
  }

  LCDPrint("Data sent", 2);
  http.end();
  return true;
}

bool checkEnv()
{
  Serial.println("Checking Env");
  envConf = readFileToString("/environment.json");
  if (envConf.isEmpty())
    return false;

  DeserializationError error = deserializeJson(envJson, envConf);

  if (error)
  {
    SPIFFS.remove("/environment.json");
    return false;
  }
  return true;
}