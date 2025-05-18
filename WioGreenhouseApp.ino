/**
 * WioGreenhouseApp.ino
 * Implementation of the WioGreenhouseApp class.
 * (c) 2025 Diego Quesada
 */

#include "WioGreenhouseApp.h"
#include <ESP8266WiFi.h>
#include "PubSubClient.h"
#include <time.h>
#include <TZ.h>
#include "secrets.h"

const int ledPin = LED_BUILTIN; // Built-in LED, turned on if all good
const int relayPin[] = { 12, 13 }; // Relay pins
const int enablePin = 15; // Enable power to other pins
const uint8_t MAX_RELAYS = 2;

const char versionString[] = "WioGreenhouse 0.11";

IPAddress mqttServer(192,168,1,84);
const uint16_t mqttPort = 1883;
const char *sensorsTopic = "sensors";
const char *relayTopic = "relays";
const char *clientID = "wioclient1";

#define RTC_UTC_TEST 1743198195
#define MYTZ TZ_America_Toronto

/*static*/ WioGreenhouseApp *WioGreenhouseApp::_singleton = nullptr;

WioGreenhouseApp::WioGreenhouseApp() :
    _pubSubClient(mqttServer, mqttPort, mqttCallback, _wifiClient),
    _webServer(*this),
    _pubSubTimer(PUBSUB_INTERVAL)
{
    _singleton = this;
    _powerSavingEnabled = true;
}

void WioGreenhouseApp::setup()
{
  pinMode(enablePin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  if (!_powerSavingEnabled) // Relays disabled in power saving mode
  {
    pinMode(relayPin[0], OUTPUT);
    pinMode(relayPin[1], OUTPUT);
    digitalWrite(relayPin[0], LOW); // Turn relay off
    digitalWrite(relayPin[1], LOW); // Turn relay off
  }
  digitalWrite(enablePin, HIGH); // Enable power to Grove connectors

  Serial.begin(115200);
  delay(500); // allow serial port time to connect

  Serial.println(versionString);

  _devices.setup();

  configTime(MYTZ, "pool.ntp.org");

  initWifi();
  connectMQTT();

  if (!_powerSavingEnabled)
  {
    initHTTPServer();
  }
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

/**
 * Connects to the MQTT broker.
 */
bool WioGreenhouseApp::connectMQTT()
{
  if (!_pubSubClient.connected() && _pubSubTimer.IsItTime())
  {
    printTime();
    Serial.println("Attempting to connect to MQTT.");

    printTime();
    if (_pubSubClient.connect(clientID, mqttUserName, mqttPassword))
    {
      Serial.println("Connected to MQTT broker.");
    }
    else
    {
      Serial.print("Connection to MQTT broker failed, error: ");
      Serial.println(_pubSubClient.state());
    }

    _pubSubTimer.Reset();
  }

  if (_pubSubClient.connected())
  {
    digitalWrite(ledPin, 1);
    _mqttConnected = true;

    // Update sysInfo topic
    char tempJson[64] = { 0 };
    sprintf(tempJson, "{ \"version\": \"%s\", \"uptime\": \"%s\" }", getVersionStr().c_str(), getBootupTime().c_str());
    pushUpdate("sysInfo", tempJson);

    return true;
  }
  else
  {
    digitalWrite(ledPin, 0);
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
  char tempJson[80] = { 0 };

  // Sensors update occurs on a set interval.
  unsigned char sensorsUpdate = _devices.updateSensors();
  if (sensorsUpdate == 1)
  {
    getSensorsJson(tempJson);
    pushUpdate(sensorsTopic, tempJson);
  }

  if (_powerSavingEnabled)
  {
    printTime(); Serial.println("Going to sleep for 5 minutes.");
    delay(500); // Allow time for the last message to be sent
    ESP.deepSleep(5*60e6);
  }
  else
  {
    // Update relays if we updated sensors, or have been overridden
    if (sensorsUpdate != 2 ||
        _relayOverride[0] != 0 || _relayOverride[1] != 0)
    {
      bool relay1Changed = updateRelay(0);
      bool relay2Changed = updateRelay(1);
      if (relay1Changed || relay2Changed)
      {
        sprintf(tempJson, "{ \"relay1\": %d, \"relay2\": %d }", _relayState[0], _relayState[1]);
        pushUpdate(relayTopic, tempJson);
      }
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
    sprintf(tempTopic, "wioLink/%x/%s", getSerialNumber(), topic, retained);
    if (_pubSubClient.publish(tempTopic, json, retained))
    {
      printTime();
      Serial.println("MQTT update sent for " + String(tempTopic));
    }

    return true;
  }
  else
  {
    printTime();
    Serial.println("MQTT not connected.");
    return false;
  }
}

/**
 * Returns a JSON string with the current sensor readings.
 */
void WioGreenhouseApp::getSensorsJson(char *jsonOut) const
{
  sprintf(jsonOut, 
    "{ \"temperature\": %.2f, \"humidity\": %.2f, \"lux\": %lu }",
    _devices.getTemp(), _devices.getHum(), _devices.getLux() );
}

/**
 * Sets the relay on or off, and updates relays MQTT topic.
 * @param Index of the relay to update
 * @return true if relay state changed, false otherwise.
 */
bool WioGreenhouseApp::updateRelay(uint8_t relayIndex)
{
  char tempJson[64] = { 0 };

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
      Serial.println("Turning relay " + String(relayIndex) + String(" ON"));
      digitalWrite(relayPin[relayIndex], HIGH);
    }
    else
    {
      printTime();
      Serial.println("Turning relay " + String(relayIndex) + String(" OFF"));
      digitalWrite(relayPin[relayIndex], LOW);
    }

    return true;
  }
  else
  {
    return false;
  }
}

/**
 * Receives MQTT notifications for any subscribed topics.
 */
/*static*/ void WioGreenhouseApp::mqttCallback(char* topic, byte* payload, unsigned int length)
{

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
  Serial.println("setRelay(" + String(relayIndex) + ", " + String(on) + ", " +
                         String((delay==0) ? RELAY_OVERRIDE : delay) + ")");

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
