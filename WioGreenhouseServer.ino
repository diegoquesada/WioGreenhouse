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
      String("\"sensorsOK\":")        + String(_app.areSensorsOK() ? "true }" : "false }"));
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
    if (arg(0) == "yes")
    {
      _app.setRelay(true);
      send(200, "text/plain", "Relay turned on for 1hr.\n");
    }
    else if (arg(0) == "no")
    {
      _app.setRelay(false);
      send(200, "text/plain", "Relay turned off for 1hr.\n");
    }
  }
}