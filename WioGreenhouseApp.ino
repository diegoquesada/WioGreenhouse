/**
 * WioGreenhouseApp.ino
 * Implementation of the WioGreenhouseApp class.
 * (c) 2025 Diego Quesada
 */

#include "WioGreenhouseApp.h"
#include <ESP8266WiFi.h>
#include "PubSubClient.h"
#include <time.h>
#include "secrets.h"

const int ledPin = LED_BUILTIN; // Built-in LED, turned on if all good
const int relayPin[] = { 12, 13 }; // Relay pins
const int enablePin = 15; // Enable power to other pins
const uint8_t MAX_RELAYS = 2;

const char versionString[] = "WioGreenhouse 0.10";

IPAddress mqttServer(192,168,1,84);
const uint16_t mqttPort = 1883;
const char *sensorsTopic = "sensors";
const char *relayTopic = "relays";
const char *clientID = "wioclient1";

const int stdTimeOffset = -5 * 60 * 60; // 5 hours offset (in seconds) standard time
const int dstTimeOffset = -4 * 60 * 60; // 4 hours offset (in seconds) daylight savings time


/*static*/ WioGreenhouseApp *WioGreenhouseApp::_singleton = nullptr;

WioGreenhouseApp::WioGreenhouseApp() :
    _pubSubClient(mqttServer, mqttPort, mqttCallback, _wifiClient),
    _webServer(*this),
    _timeClient(_ntpUDP, stdTimeOffset),
    _pubSubTimer(PUBSUB_INTERVAL)
{
    _singleton = this;
}

void WioGreenhouseApp::setup()
{
  pinMode(enablePin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(relayPin[0], OUTPUT);
  pinMode(relayPin[1], OUTPUT);
  digitalWrite(enablePin, HIGH); // Enable power to Grove connectors
  digitalWrite(relayPin[0], LOW); // Turn relay off
  digitalWrite(relayPin[1], LOW); // Turn relay off

  Serial.begin(115200);
  delay(500); // allow serial port time to connect

  Serial.println(versionString);
  
  _devices.setup();
  _timeClient.begin();

  initWifi();
  connectMQTT();
  initHTTPServer();
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
 * Currently supported: /status, /time and /relay APIs.
 * TODO: APIs to read temp, humidity and lux.
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

  if (sensorsUpdate != 2) // update time as frequently as we poll sensors
  {
    updateTime();
    if (_bootupTime == 0 && _timeClient.isTimeSet())
    {
      _bootupTime = _timeClient.getEpochTime();
    }
  }

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
 * Updates the internal time from NTP server.
 */
void WioGreenhouseApp::updateTime()
{
  if (_timeClient.update())
  {
    time_t now = _timeClient.getEpochTime();

    // Apply DST if needed
    struct tm *timeinfo = localtime(&now);
    bool dstOn = (timeinfo->tm_mon  >  2 &&  timeinfo->tm_mon < 10) ||
                 (timeinfo->tm_mon ==  2 && (timeinfo->tm_mday - timeinfo->tm_wday) > 7) ||
                 (timeinfo->tm_mon == 10 && (timeinfo->tm_mday - timeinfo->tm_wday) < 7);
    if (dstOn)
    {
      _timeClient.setTimeOffset(dstTimeOffset);
    }
    else
    {
      _timeClient.setTimeOffset(stdTimeOffset);
    }
  }
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
    _relayState[relayIndex] = _timeClient.getHours() > relayOnTime[relayIndex] && _timeClient.getHours() < relayOffTime[relayIndex];
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
  if (_timeClient.isTimeSet())
  {
    time_t now = _timeClient.getEpochTime();
    struct tm *timeinfo = localtime(&now);
    return String(asctime(timeinfo));
  }
  else
  {
    return "Time not set";
  }
}

String WioGreenhouseApp::getBootupTime()
{
  if (_timeClient.isTimeSet())
  {
    unsigned long elapsed = _timeClient.getEpochTime() - _bootupTime;
    unsigned long days = elapsed / 86400;
    unsigned long hours = (elapsed % 86400) / 3600;
    unsigned long minutes = (elapsed % 3600) / 60;
    return String(days) + "d " + String(hours) + "hr " + String(minutes) + "min";
  }
  else
  {
    return "NA";
  }
}

void WioGreenhouseApp::printTime()
{
  if (_timeClient.isTimeSet())
  {
    Serial.print("[");
    Serial.print(_timeClient.getEpochTime() - _bootupTime);
    Serial.print("] ");
  }
}
