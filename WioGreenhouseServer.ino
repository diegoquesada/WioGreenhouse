#include "WioGreenHouseServer.h"
#include "WioGreenHouseApp.h"

/*static*/ WioGreenhouseServer *WioGreenhouseServer::_singleton = nullptr;

bool WioGreenhouseServer::init()
{
  on("/", handleRoot);
  on("/status", handleStatus);
  on("/time", handleTime);
  on("/relay", handleRelay);
  
  begin();

  return true;
}

/*static*/ void WioGreenhouseServer::handleRoot()
{
    _singleton->getHomepage();
}

/*static*/ void WioGreenhouseServer::handleStatus()
{
    _singleton->getStatus();
}

/*static*/ void WioGreenhouseServer::handleTime()
{
    _singleton->getTime();
}

/*static*/ void WioGreenhouseServer::handleRelay()
{
    _singleton->setRelay();
}

void WioGreenhouseServer::getHomepage()
{
    Serial.println("HTTP server: request for root.");
    send(200, "text/html",
        "<html><head><title>GrowLights</title></head><body>" + _app.getVersionStr() + "</body></html>");
}

void WioGreenhouseServer::getStatus()
{
  if (method() == HTTP_GET)
  {
    Serial.println("HTTP server: request for /status.");
    send(200, "application/json",
      String("{ \"wifiConnected\": ") + String(_app.isWifiConnected() ? "true, " : "false, ") +
      String("\"mqttConnected\":")    + String(_app.isMqttConnected() ? "true, " : "false, ") +
      String("\"sensorsOK\":")        + String(_app.areSensorsOK() ? "true, " : "false, ") + 
      String("\"relayOn\":")          + String(_app.isRelayOn() ? "true, " : "false, ") +
      String("\"relayOverride\":")    + String(_app.getRelayOverride() ? "true, " : "false, ") +
      String("\"bootupTime\":")       + String(_app.getBootupTime()) + "}\n");
  }
}

void WioGreenhouseServer::getTime()
{
  if (method() == HTTP_GET)
  {
    send(200, "application/json", String(_app.getTime()));
  }
}

void WioGreenhouseServer::setRelay()
{
  if (method() != HTTP_POST)
  {
    send(405, "text/plain", "Method Not Allowed");
  }
  
  if (argName(0) == "on")
  {
    unsigned long delayValue = 0; // zero means don't change
    if (hasArg("delay"))
    {
      delayValue = arg("delay").toInt();
      if (delayValue < 5000) delayValue = 0; // Ignore invalid values
    }
    
    if (arg(0) == "yes")
    {
      _app.setRelay(true, delayValue);
      send(200, "text/plain", String("Relay turned on for ") + delayValue + "ms.\n");
    }
    else if (arg(0) == "no")
    {
      _app.setRelay(false, delayValue);
      send(200, "text/plain", String("Relay turned off for ") + delayValue + "ms.\n");
    }
  }
}