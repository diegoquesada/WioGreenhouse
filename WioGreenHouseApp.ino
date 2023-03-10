/**
 * WioGreenhouseApp.ino
 * Implementation of the WioGreenhouseApp class.
 * (c) Diego Quesada
 */

#include "WioGreenHouseApp.h"
#include <Digital_Light_TSL2561.h>
#include <ESP8266WiFi.h>
#include "PubSubClient.h"
#include "secrets.h"

const int ledPin = LED_BUILTIN; // Built-in LED, turned on if all good
const int relayPin = 12;
const int dhtPin = 14;
const int enablePin = 15; // Enable power to other pins

String versionString = "WioGreenhouse 0.2";

IPAddress mqttServer(10,0,0,42);
const uint16_t mqttPort = 1883;
const char *tempTopic = "sensors/greenhouse/temperature";
const char *humidityTopic = "sensors/greenhouse/humidity";
const char *lightTopic = "sensors/greenhouse/light";
const char *clientID = "wioclient1";
const char *mqttUserName = "wiolink1";
const char *mqttPassword = "elendil";


WioGreenhouseApp::WioGreenhouseApp() :
    _dht(dhtPin, DHT11),
    _pubSubClient(mqttServer, mqttPort, mqttCallback, _wifiClient),
    _webServer(this)
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

  Serial.begin(9600);

  Wire.begin();
  _dht.begin();
  TSL2561.init();

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
  if (!_pubSubClient.connected())
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

  if (sensorsUpdate != 2 && timeClient.update()) // update time and relay as frequently as we poll sensors
  {
    updateRelay();
  }

  server.handleClient();

}

/**
 * Receives MQTT notifications for any subscribed topics.
 */
/*static*/ void WioGreenhouseApp::mqttCallback(char* topic, byte* payload, unsigned int length)
{

}

//--------------------------------------------------------------------------------
// HTTP server handlers

void WioGreenhouseApp::handleRoot()
{
  Serial.println("HTTP server: request for root.");
  _webServer.send(200, "text/html", "<html><head><title>GrowLights</title></head><body>" + versionString + "</body></html>");
}

void WioGreenhouseApp::handleStatus()
{
  if (_webServer.method() == HTTP_GET)
  {
    Serial.println("HTTP server: request for /status.");
    _webServer.send(200, "application/json",
      String("{ \"wifiConnected\": ") + String(wifiConnected ? "true, " : "false, ") +
      String("\"mqttConnected\":")    + String(mqttConnected ? "true, " : "false, ") +
      String("\"sensorsOK\":")        + String(sensorsOK ? "true }" : "false }"));
  }
}

void WioGreenhouseApp::handleTime()
{
  if (_webServer.method() == HTTP_GET)
  {
    _webServer.send(200, "application/json", String(timeClient.getFormattedTime()));
  }
}

void WioGreenhouseApp::handleRelay()
{
  if (_webServer.method() != HTTP_POST)
  {
    _webServer.send(405, "text/plain", "Method Not Allowed");
  }
  
  if (_webServer.argName(0) == "on")
  {
    if (_webServer.arg(0) == "yes")
    {
      _relayOverride = 1;
      _relayOverrideTime = millis();
      _webServer.send(200, "text/plain", "Relay turned on for 1hr.\n");
    }
    else if (server.arg(0) == "no")
    {
      _relayOverride = 2;
      _relayOverrideTime = millis();
      _webServer.send(200, "text/plain", "Relay turned off for 1hr.\n");
    }
  }
}
