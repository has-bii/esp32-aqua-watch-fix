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

// Constants
#define ESPADC 4095.0
#define ESPVOLTAGE 3.3
#define BOOT_BUTTON 0
#define TEMPERATURE_PIN 4
#define PH_PIN 35
#define TURBIDITY_PIN 34
#define DO_PIN 32
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

// Function Declarations
float getTemperature();
float getVoltage(uint8_t);
float getPh();
float getTurbidity();
void handleButtonPress();
void printMenu();
void LCDPrint(const String &, int);
void connectWifi();

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
        Serial.println("getting time");
        timeClient.update();
      }
      else
        Serial.println(timeClient.getFormattedTime());
    }
    else
    {
      connectWifi();
    }

    temperature = getTemperature();
    phValue = getPh();
    turbidity = getTurbidity();
    dissolvedOxygen = getDO(TURBIDITY_PIN, int(temperature));

    Serial.println("Temp: " + String(temperature) + "\tpH: " + String(phValue) + "\tTurbidity: " + String(int(turbidity)) + "\%\tDissolved Oxygen: " + String(dissolvedOxygen));
    printMenu();

    if (timeClient.getMinutes() % 5 == 0)
    {
      if (!isSent)
      {
        sentCount++;
        Serial.print("Sending data: ");
        Serial.println(sentCount);
        isSent = true;
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
      Menu = (Menu % 4) + 1;
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

  return ph.readPH(analogRead(PH_PIN) / 4095.0 * 3300, temperature);
}

float getTurbidity()
{
  return map(analogRead(TURBIDITY_PIN), 0, 2450, 0, 100);
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
    lcd.setCursor(11, 0);
    lcd.printf("%4.0f%%", turbidity);
    lcd.setCursor(0, 1);
    lcd.print(String(dissolvedOxygen) + "mg/L");
    lcd.setCursor(10, 1);
    lcd.print(String(phValue) + "pH");
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