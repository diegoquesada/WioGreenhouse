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
const int relayPin = 13;
const int enablePin = 15; // Enable power to other pins

String versionString = "WioGreenhouse 0.3";

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
  pinMode(relayPin, OUTPUT);
  digitalWrite(enablePin, HIGH); // Enable power to Grove connectors
  digitalWrite(relayPin, LOW); // Turn relay off

  _devices.setup();

  Serial.begin(9600);
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
    Serial.println("Attempting to connect to MQTT.");

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
    return versionString;
}

void WioGreenhouseApp::loop()
{
  // Sensors update occurs on a set interval.
  unsigned char sensorsUpdate = _devices.updateSensors();
  if (sensorsUpdate == 1)
  {
    pushUpdate();
  }

  if (sensorsUpdate != 2) // update time and relay as frequently as we poll sensors
  {
    _timeClient.update();
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
    Serial.println("Relay override has expired, setting back.");
  }

  if (_relayOverride != 0) // Check again, might have expired above.
  {
    _relayState = (_relayOverride == 1);
  }
  else
  {
    _relayState = _timeClient.getHours() > 6 && _timeClient.getHours() < 20;
  }

  if (_relayState)
  {
    digitalWrite(relayPin, HIGH);
  }
  else
  {
    digitalWrite(relayPin, LOW);
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
  if (on)
  {
    _relayOverride = 1;
    if (delay != 0) _relayTimer.setDelay(delay);
    _relayTimer.Reset();
  }
  else
  {
    _relayOverride = 2;
    if (delay != 0) _relayTimer.setDelay(delay);
    _relayTimer.Reset();
  }
}
