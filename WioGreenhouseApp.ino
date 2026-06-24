/**
 * WioGreenhouseApp.ino
 * Implementation of the WioGreenhouseApp class.
 * (c) 2025 Diego Quesada
 */

#include "WioGreenhouseApp.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include "PubSubClient.h"
#include "mDNSResolver.h"
#include <ArduinoJson.h>
#include <time.h>
#include <TZ.h>
#include "secrets.h"

const int ledPin = LED_BUILTIN; // Built-in LED, turned on if all good
const int fanPin = 2; // Fan pin, happens to be the same as built-in LED
const int relayPin[] = { 12, 13 }; // Relay pins
const int enablePin = 15; // Enable power to other pins
const uint8_t MAX_RELAYS = 2;

const char versionString[] = "WioGreenhouse 2026.3";

WiFiUDP udp;
mDNSResolver::Resolver resolver(udp);

IPAddress mqttServer(192,168,1,88);
const uint16_t mqttPort = 1883;
const char *sensorsTopic = "sensors";
const char *relayTopic = "relays";
const char *clientID = "wioclient1";

#define RTC_UTC_TEST 1743198195
#define MYTZ TZ_America_Toronto

/*static*/ WioGreenhouseApp *WioGreenhouseApp::_singleton = nullptr;

char *_tempJson = nullptr;

WioGreenhouseApp::WioGreenhouseApp() :
    _devices(DEFAULT_UPDATE_INTERVAL),
    _powerTimer(POWER_INTERVAL),
    _fanTimer(FAN_INTERVAL),
    _pubSubClient(_wifiClient),
    _webServer(*this)
{
    _singleton = this;
    //_powerSavingEnabled = true;
    _tempJson = new char[80];
}

void WioGreenhouseApp::setup()
{
  pinMode(enablePin, OUTPUT);
  //pinMode(ledPin, OUTPUT);
  if (!_powerSavingEnabled) // Relays disabled in power saving mode
  {
    pinMode(relayPin[0], OUTPUT);
    pinMode(relayPin[1], OUTPUT);
    pinMode(fanPin, OUTPUT);
    digitalWrite(relayPin[0], LOW); // Turn relay off
    digitalWrite(relayPin[1], LOW); // Turn relay off
    digitalWrite(fanPin, LOW); // Turn fan off
  }
  digitalWrite(enablePin, HIGH); // Enable power to Grove connectors

  Serial.begin(115200);
  delay(500); // allow serial port time to connect

  Serial.println(versionString);

  _devices.setup();

  configTime(MYTZ, "pool.ntp.org");

  initWifi();
  initmDNS();
  connectMQTT();

  if (!_powerSavingEnabled)
  {
    initHTTPServer();
  }

  _powerTimer.Reset();
}

/**
 * Initializes wifi on the ESP8266. Blocks until connection established.
 * TODO: handle failure to connect.
 */
void WioGreenhouseApp::initWifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wifi");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  printTime();
  Serial.print("Connected to ");
  Serial.print(ssid);
  Serial.print(", IP address: ");
  Serial.println(WiFi.localIP());

  _wifiConnected = true;
}

void WioGreenhouseApp::initmDNS()
{
  if (!MDNS.begin(WiFi.hostname()))
  {
    Serial.println("Failed to start MDNS responder.");
    return;
  }

  if (!_powerSavingEnabled)
  {
    MDNS.addService("tcp", "tcp", 80);
    MDNS.addService("http", "tcp", 80);
  }

  resolver.setLocalIP(WiFi.localIP());

  IPAddress brokerIP = resolver.search("Claudius.local");
  if (brokerIP != INADDR_NONE)
  {
    mqttServer = brokerIP;
    _pubSubClient.setServer(mqttServer, mqttPort);
    _pubSubClient.setCallback(mqttCallback);

    printTime();
    Serial.print("Resolved MQTT broker mDNS address to IP: ");
    Serial.println(mqttServer);
  }
  else
  {
    printTime();
    Serial.println("Failed to resolve mDNS address for MQTT broker, using default IP.");
  }
}

/**
 * Connects to the MQTT broker.
 */
bool WioGreenhouseApp::connectMQTT()
{
  if (!_pubSubClient.connected())
  {
    printTime(); Serial.println("Attempting to connect to MQTT.");

    if (_pubSubClient.connect(clientID, mqttUserName, mqttPassword))
    {
      printTime(); Serial.println("Connected to MQTT broker.");
    }
    else
    {
      printTime();
      Serial.print("Connection to MQTT broker ");
      Serial.print(mqttServer.toString());
      Serial.print(" failed, error: ");
      Serial.println(_pubSubClient.state());
    }
  }

  if (_pubSubClient.connected())
  {
    //digitalWrite(ledPin, 1);
    _mqttConnected = true;

    // Update sysInfo topic
    char tempBuffer[64] = { 0 };
    sprintf(tempBuffer, "{ \"version\": \"%s\", \"uptime\": \"%s\" }", getVersionStr().c_str(), getBootupTime().c_str());
    pushUpdate("sysInfo", tempBuffer, true);

    sprintf(tempBuffer, "wioLink/%x/config", getSerialNumber());
    _pubSubClient.subscribe(tempBuffer);

    return true;
  }
  else
  {
    //digitalWrite(ledPin, 0);
    _mqttConnected = false;
    return false;
  }
}

/**
 * Configures and starts the HTTP server.
 */
bool WioGreenhouseApp::initHTTPServer()
{
  printTime();
  if (_webServer.init())
  {
      Serial.println("HTTP webServer started.");
      return true;
  }
  else
  {
      Serial.println("Failed to initialize HTTP webServer");
      return false;
  }
}

String WioGreenhouseApp::getVersionStr() const
{
    return (const char *)versionString;
}

uint32_t WioGreenhouseApp::getSerialNumber() const
{
  uint8_t mac[6];
  wifi_get_macaddr(STATION_IF, mac);
  return (uint32_t)mac[2] << 24 | (uint32_t)mac[3] << 16 | (uint32_t)mac[4] << 8 | mac[5];
}

void WioGreenhouseApp::loop()
{
  unsigned char sensorsUpdate = _devices.updateSensors();
  if (sensorsUpdate == 1)
  {
    /*printTime();
    Serial.print("Pushing sensor update, free heap: ");
    Serial.println(ESP.getFreeHeap()); */

    getSensorsJson(_tempJson, 80);
    pushUpdate(sensorsTopic, _tempJson);
    
    yield(); // Feed watchdog periodically
  }
  
  _pubSubClient.loop();

  if (_powerSavingEnabled && _powerTimer.IsItTime())
  {
    // The following actions are not performed while in power saving mode:
    // - Relay control (relays always off)
    // - Fan control (fan always off)
    // - HTTP server
    printTime(); Serial.println("Going to sleep for 5 minutes.");
    delay(100); // Allow time for the last message to be sent
    ESP.deepSleep(DEFAULT_UPDATE_INTERVAL*1000);
  }

  if (!_powerSavingEnabled)
  {
    // Update relays and fan if we updated sensors, or have been overridden.
    if (sensorsUpdate != 2 ||
        _relayOverride[0] != 0 || _relayOverride[1] != 0 || _fanOverride != 0)
    {
      /*printTime();
      Serial.print("Updating relays, free heap: ");
      Serial.println(ESP.getFreeHeap());*/

      bool relay1Changed = updateRelay(0);
      bool relay2Changed = updateRelay(1);
      if (relay1Changed || relay2Changed)
      {
        getRelaysJson(_tempJson, 80);
        pushUpdate(relayTopic, _tempJson);
      }

      updateFan();

      yield(); // Feed watchdog periodically
    }

    _webServer.handleClient();
  }
}

/**
 * Uploads sensor readings to MQTT broker.
 */
bool WioGreenhouseApp::pushUpdate(const char *topic, const char *json, bool retained /*=false*/)
{
  // Make sure the MQTT client is up and running.
  if (!_pubSubClient.connected())
  {
    connectMQTT();
  }

  if (_pubSubClient.connected())
  {
    char tempTopic[64] = { 0 };
    snprintf(tempTopic, sizeof(tempTopic), "wioLink/%x/%s", getSerialNumber(), topic);
    if (_pubSubClient.publish(tempTopic, json, retained))
    {
      printTime(); Serial.print("MQTT update sent for "); Serial.println(tempTopic);
    }

    return true;
  }
  else
  {
    printTime(); Serial.println("MQTT not connected, no update sent.");
    return false;
  }
}

/**
 * Returns a JSON string with the current sensor readings.
 */
void WioGreenhouseApp::getSensorsJson(char *jsonOut, size_t bufferSize) const
{
  snprintf(jsonOut, bufferSize,
    "{ \"temperature\": %.2f, \"humidity\": %.2f, \"lux\": %lu }",
    _devices.getTemp(), _devices.getHum(), _devices.getLux() );
}

void WioGreenhouseApp::getRelaysJson(char *jsonOut, size_t bufferSize) const
{
  snprintf(jsonOut, bufferSize,
    "{ \"relay1\": %d, \"relay2\": %d }",
    _relayState[0], _relayState[1] );
}

/**
 * Sets the relay on or off, and updates relays MQTT topic.
 * @param Index of the relay to update
 * @return true if relay state changed, false otherwise.
 */
bool WioGreenhouseApp::updateRelay(uint8_t relayIndex)
{
  if (relayIndex >= MAX_RELAYS)
  {
    printTime();
    Serial.println("Invalid relay index");
    return false;
  }

  // Relay has been overriden and it's time to set it back.
  if (_relayOverride[relayIndex] != 0 && _relayTimer[relayIndex].IsItTime())
  {
    _relayOverride[relayIndex] = 0;
    printTime();
    Serial.println("Relay override has expired, setting back.");
  }

  bool prevRelayState = _relayState[relayIndex];

  if (_relayOverride[relayIndex] != 0) // Check override again, might have expired above.
  {
    _relayState[relayIndex] = (_relayOverride[relayIndex] == 1); // 1 means overridden to on, 2 means overridden to off
  }
  else if (relayOnTime[relayIndex] == RELAY_ALWAYSON) // Special case for always on
  {
    _relayState[relayIndex] = true;
  }
  else // Normal case where relay is driven by time
  {
    time_t now = time(nullptr);
    tm *tm = localtime(&now);
    _relayState[relayIndex] = tm->tm_hour > relayOnTime[relayIndex] && tm->tm_hour < relayOffTime[relayIndex];
  }

  // If the relay state changed, set pin and push to MQTT.
  if (_relayState[relayIndex] != prevRelayState)
  {
    if (_relayState[relayIndex])
    {
      printTime();
      Serial.print("Turning relay "); Serial.print(relayIndex); Serial.println(" ON");
      digitalWrite(relayPin[relayIndex], HIGH);
    }
    else
    {
      printTime();
      Serial.print("Turning relay "); Serial.print(relayIndex); Serial.println(" OFF");
      digitalWrite(relayPin[relayIndex], LOW);
    }

    return true;
  }
  else
  {
    return false;
  }
}

bool WioGreenhouseApp::updateFan()
{
  if (_fanOverride != 0)
  {
    return false;
  }

  if (_devices.getHum() > 70 && !_fanOn)
  {
    Serial.println("Turning fan ON");
    digitalWrite(fanPin, HIGH);
    _fanTimer.Reset();
    _fanOn = true;
  }
  else if (_fanOn && _fanTimer.IsItTime())
  {
    Serial.println("Turning fan OFF");
    digitalWrite(fanPin, LOW);
    _fanOn = false;
  }

  return true;
}

/**
 * Receives MQTT notifications for any subscribed topics.
 */
/*static*/ void WioGreenhouseApp::mqttCallback(char* topic, byte* payload, unsigned int length)
{
  WioGreenhouseApp::getApp().handleMQTTMessage(topic, payload, length);
}

void WioGreenhouseApp::handleMQTTMessage(char* topic, byte* payload, unsigned int length)
{
/*  printTime();
  Serial.print("--- Received MQTT message on topic: ");
  Serial.print(topic);
  Serial.println(" ---");*/
  
  // Ignore absurdly large payloads to avoid exhausting heap.
  if (length >= 512)
  {
    printTime(); Serial.println("MQTT payload too large");
    return;
  }
  
  // Build the expected config topic: wioLink/{device_id}/config
  char expectedTopic[64] = { 0 };
  sprintf(expectedTopic, "wioLink/%x/config", getSerialNumber());
  
  // Check if this is the config topic for this device
  if (strcmp(topic, expectedTopic) != 0)
  {
    return;
  }

  // Create a buffer for the payload (null-terminated)
  char* payloadStr = new char[length + 1];
  if (!payloadStr)
  {
    printTime(); Serial.println("MQTT payload allocation failed");
    return;
  }
  memcpy(payloadStr, payload, length);
  payloadStr[length] = '\0';
  
  // Parse JSON payload
  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, payloadStr);
  
  if (error)
  {
    printTime(); Serial.println("Failed to parse JSON config payload");
    delete[] payloadStr;
    return;
  }

  yield(); // Feed watchdog after parsing MQTT message

  // Read "relays" array
  if (doc.containsKey("relays") && doc["relays"].is<JsonArray>())
  {
    JsonArray relaysArray = doc["relays"];
    for (size_t i = 0; i < relaysArray.size() && i < MAX_RELAYS; i++)
    {
      if (relaysArray[i].is<JsonObject>())
      {
        JsonObject relayConfig = relaysArray[i];
        if (relayConfig.containsKey("on") && relayConfig["on"] == "time")
        {
          int timeOn = relayConfig["timeOn"].as<int>();
          int timeOff = relayConfig["timeOff"].as<int>();
          relayOnTime[i] = timeOn;
          relayOffTime[i] = timeOff;
          //printTime(); Serial.println("relay " + String(i) + ": timeOn=" + String(timeOn) + ", timeOff=" + String(timeOff));
        }
        else if (relayConfig.containsKey("on") && relayConfig["on"] == "always")
        {
          relayOnTime[i] = RELAY_ALWAYSON;
          //printTime(); Serial.println("relay " + String(i) + ": always on");
        }
        else
        {
          //printTime(); Serial.println("Invalid config for relay " + String(i));
        }
      }
    }
  }

  if (doc.containsKey("fan"))
  {
    if (doc["fan"] == "on")
    {
      _fanOn = true;
      digitalWrite(fanPin, HIGH);
      _fanOverride = 1;
      printTime(); Serial.println("Fan turned ON via config");
    }
    else if (doc["fan"] == "off")
    {
      _fanOn = false;
      digitalWrite(fanPin, LOW);
      _fanOverride = 2;
      printTime(); Serial.println("Fan turned OFF via config");
    }
  }

  // Read "powerSaving" value
  if (doc.containsKey("powerSaving"))
  {
    _powerSavingEnabled = doc["powerSaving"].as<bool>();
    //printTime(); Serial.println("powerSaving: " + String(_powerSavingEnabled ? "enabled" : "disabled"));
  }
  
  // Read "device_name" value
  if (doc.containsKey("deviceName"))
  {
    const char* deviceName = doc["deviceName"];
    strncpy(_deviceName, deviceName, sizeof(_deviceName) - 1);
    _deviceName[sizeof(_deviceName) - 1] = '\0';
    //printTime(); Serial.println("deviceName: " + String(deviceName));
  }
  
  delete[] payloadStr;
}

/**
 * Sets or unsets the relay for a specified amount of time.
 * @param delay Amount of time until relay resets back, in ms. Pass 0 to leave the delay unchanged.
*/
void WioGreenhouseApp::setRelay(uint8_t relayIndex, bool on, unsigned long delay)
{
  if (relayIndex >= MAX_RELAYS)
  {
    printTime();
    Serial.println("Invalid relay index");
    return;
  }

  printTime();
  Serial.print("setRelay(");
  Serial.print(relayIndex);
  Serial.print(", ");
  Serial.print(on);
  Serial.print(", ");
  Serial.print((delay==0) ? RELAY_OVERRIDE : delay);
  Serial.println(")");

  if (on)
  {
    _relayOverride[relayIndex] = 1;
    _relayTimer[relayIndex].setDelay(delay == 0 ? RELAY_OVERRIDE : delay);
    _relayTimer[relayIndex].Reset();
  }
  else
  {
    _relayOverride[relayIndex] = 2;
    _relayTimer[relayIndex].setDelay(delay == 0 ? RELAY_OVERRIDE : delay);
    _relayTimer[relayIndex].Reset();
  }
}

String WioGreenhouseApp::getDate() const
{
  time_t now = time(nullptr);
  return String(ctime(&now));
}

String WioGreenhouseApp::getTime() const
{
  time_t now = time(nullptr);
  tm *tm = localtime(&now);
  return String(tm->tm_hour) + ":" +
         String(tm->tm_min) + ":" +
         String(tm->tm_sec);
}

String WioGreenhouseApp::getBootupTime()
{
  unsigned long seconds = millis() / 1000;
  unsigned long days = seconds / 86400;
  unsigned long hours = (seconds % 86400) / 3600;
  unsigned long minutes = (seconds % 3600) / 60;
  return String(days) + "d " + String(hours) + "hr " + String(minutes) + "min";
}

void WioGreenhouseApp::printTime()
{
  Serial.print("[");
  Serial.print(millis()/1000);
  Serial.print("] ");
}

String WioGreenhouseApp::getBootReasonString() const
{
  struct rst_info *rst_info = system_get_rst_info();
  String reasonString = String(rst_info->reason);
  if (rst_info->reason == REASON_EXCEPTION_RST)
  {
    reasonString += ", " + String(rst_info->exccause);
  }
  return reasonString;
}
