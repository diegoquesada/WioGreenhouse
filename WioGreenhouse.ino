/**
 * WioLink ESP8266 project to monitor and control a greenhouse.
 * Copyright 2022 Diego Quesada
 */

#include <WiFiUdp.h>
#include <NTPClient.h>
#include "WioGreenhouseApp.h"

IPAddress mqttServer(10,0,0,42);
const uint16_t mqttPort = 1883;
const char *tempTopic = "sensors/greenhouse/temperature";
const char *humidityTopic = "sensors/greenhouse/humidity";
const char *lightTopic = "sensors/greenhouse/light";
const char *clientID = "wioclient1";
const char *mqttUserName = "wiolink1";
const char *mqttPassword = "elendil";

WiFiUDP ntpUDP;
const int timeOffset = -4 * 60 * 60; // 5 hours offset (in seconds) during DST
NTPClient timeClient(ntpUDP, timeOffset);


const unsigned long UPDATE_INTERVAL = 30000; // How often we should update sensors in ms.
unsigned long lastUpdateTime = 0;

//--------------------------------------------------------------------------------
// Initialization

WioGreenhouseApp app;

void setup()
{
  app.setup();
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
  _app.loop();
}

