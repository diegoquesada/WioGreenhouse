/**
 * WioLink ESP8266 project to monitor and control a greenhouse.
 * Copyright 2022 Diego Quesada
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include "PubSubClient.h"
#include "DHT.h"
#include <Digital_Light_TSL2561.h>
#include <NTPClient.h>

String versionString = "WioGreenhouse 0.1";

const int ledPin = LED_BUILTIN; // Built-in LED, turned on if all good
const int relayPin = 12;
const int dhtPin = 14;
const int enablePin = 15;

const char *ssid = "";
const char *password = "";

IPAddress mqttServer(10,0,0,42);
const uint16_t mqttPort = 1883;
const char *tempTopic = "sensors/greenhouse/temperature";
const char *humidityTopic = "sensors/greenhouse/humidity";
const char *lightTopic = "sensors/greenhouse/light";
const char *clientID = "wioclient1";
const char *mqttUserName = "wiolink1";
const char *mqttPassword = "elendil";

void mqttCallback(char* topic, byte* payload, unsigned int length);
bool connectMQTT();

WiFiClient wifiClient;
PubSubClient pubSubClient(mqttServer, mqttPort, mqttCallback, wifiClient);

WiFiUDP ntpUDP;
const int timeOffset = -4 * 60 * 60; // 5 hours offset (in seconds) during DST
NTPClient timeClient(ntpUDP, timeOffset);

ESP8266WebServer server(80);

bool initHTTPServer();
void handleRoot();
void handleStatus();
void handleTime();
void handleRelay();

DHT dht(dhtPin, DHT11);
float temp_hum_val[2] = {0}; // Temp and humidity from DHT sensor -- 0: humidity, 1: temp
long lux = 0;

bool wifiConnected = false;
bool mqttConnected = false;
bool sensorsOK = false;
bool relayState = false;

const unsigned long UPDATE_INTERVAL = 30000; // How often we should update sensors in ms.
unsigned long lastUpdateTime = 0;

const unsigned long RELAY_OVERRIDE = 60*60*60; // If relay overriden via API, this is how long the override will hold.
unsigned short relayOverride = 0; // 0 if not overridden (driven by time), 1 is override on, 2 is override off
unsigned long relayOverrideTime = 0; // When the override was set

//--------------------------------------------------------------------------------
// Initialization

void setup()
{
  pinMode(enablePin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(enablePin, HIGH); // Enable power to Grove connectors
  digitalWrite(relayPin, LOW); // Turn relay off

  Serial.begin(9600);

  Wire.begin();
  dht.begin();
  TSL2561.init();

  delay(500); // allow serial port time to connect

  Serial.println(versionString);
  
  initWifi();
  connectMQTT();
  initHTTPServer();

  timeClient.begin();
}

/**
 * Receives MQTT notifications for any subscribed topics.
 */
void mqttCallback(char* topic, byte* payload, unsigned int length)
{
}

/**
 * Initializes wifi on the ESP8266. Blocks until connection established.
 * TODO: handle failure to connect.
 */
void initWifi()
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

  wifiConnected = true;
}

/**
 * Connects to the MQTT broker.
 */
bool connectMQTT()
{
  if (!pubSubClient.connected())
  {
    Serial.println("Attempting to connect to MQTT.");

    if (pubSubClient.connect(clientID, mqttUserName, mqttPassword))
    {
      Serial.println("Connected to MQTT broker");
    }
    else
    {
      Serial.print("Connection to MQTT broker failed, error: ");
      Serial.println(pubSubClient.state());
    }
  }

  if (pubSubClient.connected())
  {
    digitalWrite(ledPin, 1);
    mqttConnected = true;
    return true;
  }
  else
  {
    digitalWrite(ledPin, 0);
    mqttConnected = false;
    return false;
  }
}

/**
 * Configures and starts the HTTP server.
 * Currently supported: /status, /time and /relay APIs.
 * TODO: APIs to read temp, humidity and lux.
 */
bool initHTTPServer()
{
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/time", handleTime);
  server.on("/relay", handleRelay);
  
  server.begin();
  Serial.println("HTTP server started.");

  return true;
}

//--------------------------------------------------------------------------------
// Sensor updates

/**
 * Takes readings from temp, humidity and lux sensors.
 * Updates sensorsOK global variable to indicate whether all sensors are working properly.
 * @return 0 if at least one sensor could not be retrieved successfully
 *         1 if all sensors were retreaved successfully
 *         2 if it is not yet time to update
 */
unsigned char updateSensors()
{
  unsigned long currentTime = millis();
  bool needsUpdate = false;

  if (lastUpdateTime == 0) // special case: first time
  {
    needsUpdate = true;
  }
  else if (currentTime < lastUpdateTime) // we wrapped
  { 
    needsUpdate = (ULONG_MAX - lastUpdateTime + 1 + currentTime) > UPDATE_INTERVAL;
  }
  else
  {
    needsUpdate = (currentTime - lastUpdateTime) > UPDATE_INTERVAL;
  }

  if (needsUpdate)
  {
    if (!dht.readTempAndHumidity(temp_hum_val))
    {
      lux = TSL2561.readVisibleLux();
      
      Serial.print("Humidity: ");
      Serial.print(temp_hum_val[0]);
      Serial.print(" %\tTemperature: ");
      Serial.print(temp_hum_val[1]);
      Serial.print(" *C\tLux: ");
      Serial.println(lux);
  
      sensorsOK = true;
    }
    else
    {
      Serial.println("Failed to get temperature and humidity.");
      sensorsOK = false;
    }
  
    lastUpdateTime = millis();
    if (lastUpdateTime == 0) lastUpdateTime++; // zero is a special case.

    return sensorsOK;
  }
  else
  {
    return 2;
  }
}

/**
 * Uploads sensor readings to MQTT broker.
 */
bool pushUpdate()
{
  // Make sure the MQTT client is up and running.
  if (!pubSubClient.connected())
  {
    connectMQTT();
  }

  if (pubSubClient.connected())
  {
    if (pubSubClient.publish(tempTopic, String(temp_hum_val[1], 1).c_str()))
    {
      Serial.println("Temperature sent.");
    }

    if (pubSubClient.publish(humidityTopic, String(temp_hum_val[0], 1).c_str()))
    {
      Serial.println("Humidity sent.");
    }

    if (pubSubClient.publish(lightTopic, String(lux).c_str()))
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
void updateRelay()
{
  bool doOverride = false;
  if (relayOverride != 0)
  {
    unsigned long currentTime = millis();
    if (currentTime < relayOverrideTime) // we wrapped
    { 
      doOverride = (ULONG_MAX - relayOverrideTime + 1 + currentTime) > RELAY_OVERRIDE;
    }
    else
    {
      doOverride = (relayOverrideTime - currentTime) > RELAY_OVERRIDE;
    }
  }

  if (doOverride)
  {
    relayState = (relayOverride == 1);
  }
  else
  {
    relayState = timeClient.getHours() > 6 && timeClient.getHours() < 20;
  }
  
  if (relayState)
    digitalWrite(relayPin, HIGH);
  else
    digitalWrite(relayPin, LOW);
}

void loop()
{
  // Sensors update occurs on a set interval.
  unsigned char sensorsUpdate = updateSensors();
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

//--------------------------------------------------------------------------------
// HTTP server handlers

void handleRoot()
{
  Serial.println("HTTP server: request for root.");
  server.send(200, "text/html", "<html><head><title>GrowLights</title></head><body>" + versionString + "</body></html>");
}

void handleStatus()
{
  if (server.method() == HTTP_GET)
  {
    Serial.println("HTTP server: request for /status.");
    server.send(200, "application/json",
      String("{ \"wifiConnected\": ") + String(wifiConnected ? "true, " : "false, ") +
      String("\"mqttConnected\":")    + String(mqttConnected ? "true, " : "false, ") +
      String("\"sensorsOK\":")        + String(sensorsOK ? "true }" : "false }"));
  }
}

void handleTime()
{
  if (server.method() == HTTP_GET)
  {
    server.send(200, "application/json", String(timeClient.getFormattedTime()));
  }
}

void handleRelay()
{
  if (server.method() != HTTP_POST)
  {
    server.send(405, "text/plain", "Method Not Allowed");
  }
  
  if (server.argName(0) == "on")
  {
    if (server.arg(0) == "yes")
    {
      relayOverride = 1;
      relayOverrideTime = millis();
      server.send(200, "text/plain", "Relay turned on for 1hr.\n");
    }
    else if (server.arg(0) == "no")
    {
      relayOverride = 2;
      relayOverrideTime = millis();
      server.send(200, "text/plain", "Relay turned off for 1hr.\n");
    }
  }
}
