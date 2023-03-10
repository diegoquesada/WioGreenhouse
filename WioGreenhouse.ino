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


//--------------------------------------------------------------------------------
// Initialization

WioGreenhouseApp app;

void setup()
{
  app.setup();
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
  app.loop();
}

