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
// #include <Webserverr/Webserverr.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <ESPSupabaseRealtime.h>

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
SupabaseRealtime realtime;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
// AsyncWebServer server(80);
LiquidCrystal_I2C lcd(0x27, 16, 2);
OneWire oneWire(TEMPERATURE_PIN);
DallasTemperature sensors(&oneWire);
DFRobot_PH ph;
float phValue, temperature, turbidity, dissolvedOxygen;
int Menu = 1;
bool syncEnable = true;

JsonDocument WifiJson, AquariumJson, UserJson;
String WifiString, AquariumString, UserString;

const String SUPABASE_URL = "https://ewlulhkrefobaooxmctt.supabase.co";
const String API_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImV3bHVsaGtyZWZvYmFvb3htY3R0Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3MzM4NDM5ODIsImV4cCI6MjA0OTQxOTk4Mn0.LGGDDHaAcH4f645jT3IC5-adPSku4BbRip52-Ui6e08";
const String insert_url = "https://ewlulhkrefobaooxmctt.supabase.co/rest/v1/measurements";

// Function Declarations
float getTemperature();
float getVoltage(uint8_t);
float getPh();
float getTurbidity();
void handleButtonPress();
void printMenu();
void LCDPrint(const String &, int);
bool connectWifi();
bool sendData();
bool readConfiguration();
void HandleChanges(String result);

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

  // setupWebserver(server);

  if (!readConfiguration())
    return;

  connectWifi();

  if (WiFi.status() == WL_CONNECTED)
  {
    timeClient.begin();
    timeClient.setTimeOffset(3600 * 3);
    timeClient.update();

    realtime.begin(SUPABASE_URL, API_KEY, HandleChanges);
    realtime.login_email(UserJson["email"].as<String>(), UserJson["password"].as<String>());

    realtime.addChangesListener("aquarium", "UPDATE", "public", "id=eq." + AquariumJson["id"].as<String>());

    realtime.sendPresence(AquariumJson["name"].as<String>(), timeClient.getFormattedDate(timeClient.getEpochTime() - (3 * 3600)));

    realtime.listen();
  }
}

void loop()
{
  static unsigned long timepoint = millis();
  static int sentCount;
  static bool isSent;
  handleButtonPress();

  if (millis() - timepoint > 1000U)
  {
    timepoint = millis();

    temperature = getTemperature();
    phValue = getPh();
    turbidity = getTurbidity();
    dissolvedOxygen = getDO(DO_PIN, int(temperature));

    if (WiFi.status() == WL_CONNECTED)
    {
      if (!timeClient.update())
      {
        timeClient.forceUpdate();
      }
      else
      {
        if (timeClient.getMinutes() % 5 == 0)
        {
          if (!isSent && syncEnable)
          {
            isSent = true;

            Serial.println(timeClient.getFormattedDate());
            if (sendData())
              sentCount++;
          }
        }
        else
          isSent = false;
      }
    }
    else
      connectWifi();

    printMenu();
  }

  // ph.calibration(getVoltage(PH_PIN), temperature);

  if (Serial.available() > 0)
  {
    Menu = Serial.parseInt(); // Read integer input

    while (Serial.available() > 0)
    { // Clear any remaining data in the buffer
      Serial.read();
    }
  }

  realtime.loop();
}

void HandleChanges(String result)
{
  JsonDocument doc;
  deserializeJson(doc, result);

  File file = SPIFFS.open("/aquarium.json", FILE_WRITE);

  if (!file)
  {
    Serial.println("Failed to sync aquarium settings: failed to write file");
    file.close();
    return;
  }

  serializeJson(doc["record"], file);
  file.close();

  readConfiguration();

  Serial.println("Aquarium settings synced");
}

bool readConfiguration()
{

  Serial.println("Reading configuration");

  // Read wifi configuration
  WifiString = readFileToString("/wifi.json");

  if (WifiString.isEmpty())
  {
    Serial.println("wifi.json doesn't exist!");
    return false;
  }

  DeserializationError error = deserializeJson(WifiJson, WifiString);

  if (error)
  {
    Serial.println("Failed to parse wifi configuration!");
    return false;
  }

  if (!WifiJson["ssid"].is<String>() || !WifiJson["password"].is<String>())
  {
    Serial.println("ssid and password are required in wifi.json!");
    return false;
  }

  // Read aquarium configuration
  AquariumString = readFileToString("/aquarium.json");

  if (AquariumString.isEmpty())
  {
    Serial.println("aquarium.json doesn't exist!");
    return false;
  }

  error = deserializeJson(AquariumJson, AquariumString);

  if (error)
  {
    Serial.println("Failed to parse aquarium configuration!");
    return false;
  }

  syncEnable = AquariumJson["enable_monitoring"].as<bool>();

  // Read user configuration
  UserString = readFileToString("/user.json");

  if (UserString.isEmpty())
  {
    Serial.println("user.json doesn't exist!");
    return false;
  }

  error = deserializeJson(UserJson, UserString);

  if (error)
  {
    Serial.println("Failed to parse user!");
    return false;
  }

  Serial.println("Configurations ok");
  return true;
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
  float voltage = getVoltage(PH_PIN);
  return voltage == 0 ? 0 : ph.readPH(voltage, temperature);
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
    if (timeClient.update())
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
      if (WifiJson["ssid"].is<String>())
      {
        lcd.print(WifiJson["ssid"].as<String>());
        lcd.setCursor(0, 1);
        lcd.print(WiFi.localIP().toString());
      }
    }
    else
    {
      lcd.print("Network not connected.");
    }
    break;
  case 6:
    lcd.print(AquariumJson["name"].as<String>());
    lcd.setCursor(0, 1);
    lcd.print(syncEnable ? "Sync enabled" : "Sync disabled");
    break;
  }
}

bool connectWifi()
{
  unsigned long timepoint = millis();

  Serial.println("Connecting");

  WiFi.begin(WifiJson["ssid"].as<String>(), WifiJson["password"].as<String>());

  while (millis() - timepoint < 10000U && WiFi.status() != WL_CONNECTED)
  {
    timepoint = millis();
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Connected to " + WifiJson["ssid"].as<String>());
    return true;
  }
  Serial.println("Failed to connect to " + WifiJson["ssid"].as<String>());
  return false;
}

bool sendData()
{
  HTTPClient http;
  int httpResponseCode;

  if (!AquariumJson["id"].is<String>())
  {
    Serial.println("Aquarium ID missing!");
    return false;
  }

  JsonDocument payloadJson;
  String payloadString;

  payloadJson["env_id"] = AquariumJson["id"].as<String>();
  payloadJson["temp"] = temperature;
  payloadJson["dissolved_oxygen"] = dissolvedOxygen;
  payloadJson["turbidity"] = turbidity / 100;
  payloadJson["ph"] = phValue;
  payloadJson["created_at"] = timeClient.getFormattedDate(timeClient.getEpochTime() - (3 * 3600));

  serializeJson(payloadJson, payloadString);
  http.begin(insert_url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", API_KEY);

  Serial.println("Sending measurement");
  httpResponseCode = http.POST(payloadString);

  if (httpResponseCode != 201)
  {
    Serial.println("Failed to send!");
    http.end();
    return false;
  }

  Serial.println("Measurement has been sent");
  http.end();
  return true;
}