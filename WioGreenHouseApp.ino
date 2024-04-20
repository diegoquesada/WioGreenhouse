/**
 * WioGreenhouseApp.ino
 * Implementation of the WioGreenhouseApp class.
 * (c) Diego Quesada
 */

#include "WioGreenHouseApp.h"
#include <ESP8266WiFi.h>
#include "PubSubClient.h"
#include "secrets.h"

const int ledPin = LED_BUILTIN; // Built-in LED, turned on if all good
const int relayPin1 = 12;
const int relayPin2 = 13;
const int enablePin = PIN_GROVE_POWER; // Enable power to other pins

const char versionString[] = "WioGreenhouse 0.6";

IPAddress mqttServer(10,0,0,42);
const uint16_t mqttPort = 1883;
const char *tempTopic = "sensors/greenhouse/temperature";
const char *humidityTopic = "sensors/greenhouse/humidity";
const char *lightTopic = "sensors/greenhouse/light";
const char *clientID = "wioclient1";
const char *mqttUserName = "wiolink1";
const char *mqttPassword = "elendil";


/*static*/ WioGreenhouseApp *WioGreenhouseApp::_singleton = nullptr;

WioGreenhouseApp::WioGreenhouseApp() :
    _pubSubClient(mqttServer, mqttPort, mqttCallback, _wifiClient),
    _webServer(*this),
    _timeClient(_ntpUDP, timeOffset),
    _pubSubTimer(PUBSUB_INTERVAL),
    _relayTimer(RELAY_OVERRIDE)
{
    _singleton = this;
}

void WioGreenhouseApp::setup()
{
  pinMode(enablePin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(relayPin1, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  digitalWrite(enablePin, HIGH); // Enable power to Grove connectors
  digitalWrite(relayPin1, LOW); // Turn relay off
  digitalWrite(relayPin2, LOW); // Turn relay off

  _devices.setup();

  Serial.begin(115200); // baud rate to match the Wio's bootloader
  delay(500); // allow serial port time to connect

  Serial.println(versionString);
  
  initWifi();
  connectMQTT();
  initHTTPServer();

  _timeClient.begin();
}

/**
 * Initializes wifi on the ESP8266. Blocks until connection established.
 * TODO: handle failure to connect.
 */
void WioGreenhouseApp::initWifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

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
      Serial.println("Connected to MQTT broker");
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

void WioGreenhouseApp::loop()
{
  // Sensors update occurs on a set interval.
  unsigned char sensorsUpdate = _devices.updateSensors();
  if (sensorsUpdate == 1)
  {
    pushUpdate();
  }

  if (sensorsUpdate != 2) // update time as frequently as we poll sensors
  {
    _timeClient.update();
    if (_bootupTime == 0 && _timeClient.isTimeSet())
    {
      _bootupTime = _timeClient.getEpochTime();
    }
  }

  if (sensorsUpdate != 2 || _relayOverride != 0) // update relay if we updated sensors, or have been overridden
  {
    updateRelay();
  }

  _webServer.handleClient();
}

/**
 * Uploads sensor readings to MQTT broker.
 */
bool WioGreenhouseApp::pushUpdate()
{
  // Make sure the MQTT client is up and running.
  if (!_pubSubClient.connected())
  {
    connectMQTT();
  }

  if (_pubSubClient.connected())
  {
    if (_pubSubClient.publish(tempTopic, String(_devices.getTemp(), 1).c_str()))
    {
      Serial.println("Temperature sent.");
    }

    if (_pubSubClient.publish(humidityTopic, String(_devices.getHum(), 1).c_str()))
    {
      Serial.println("Humidity sent.");
    }

    if (_pubSubClient.publish(lightTopic, String(_devices.getLux()).c_str()))
    {
      Serial.println("Lux sent.");
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
 * Sets the relay on or off, as appropriate.
 */
void WioGreenhouseApp::updateRelay()
{
  if (_relayOverride != 0 && _relayTimer.IsItTime()) // Relay has been overriden and it's time to set back.
  {
    _relayOverride = 0;
    printTime();
    Serial.println("Relay override has expired, setting back.");
  }

  bool prevRelayState = _relayState;

  if (_relayOverride != 0) // Check again, might have expired above.
  {
    _relayState = (_relayOverride == 1); // 1 means overridden to on, 2 means overridden to off
  }
  else
  {
    _relayState = _timeClient.getHours() > relay1OnTime && _timeClient.getHours() < relay1OffTime;
  }

  if (_relayState != prevRelayState)
  {
    if (_relayState)
    {
      Serial.println("Turning relay 1 ON");
      digitalWrite(relayPin1, HIGH);
    }
    else
    {
      Serial.println("Turning relay 1 OFF");
      digitalWrite(relayPin1, LOW);
    }
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
void WioGreenhouseApp::setRelay(bool on, unsigned long delay)
{
  printTime();
  Serial.print("setRelay(");
  Serial.print(on);
  Serial.print(", ");
  Serial.print(delay);
  Serial.println(")");

  if (on)
  {
    _relayOverride = 1;
    _relayTimer.setDelay(delay == 0 ? RELAY_OVERRIDE : delay);
    _relayTimer.Reset();
  }
  else
  {
    _relayOverride = 2;
    _relayTimer.setDelay(delay == 0 ? RELAY_OVERRIDE : delay);
    _relayTimer.Reset();
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