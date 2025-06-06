/**
 * WioGreenhouseServer.ino
 * HTTP server to handle status and sensor requests.
 * (c) 2025 Diego Quesada
*/

#include "WioGreenhouseServer.h"
#include "WioGreenhouseApp.h"

/*static*/ WioGreenhouseServer *WioGreenhouseServer::_singleton = nullptr;

bool WioGreenhouseServer::init()
{
  on("/", handleRoot);
  on("/status", handleStatus);
  on("/date", handleDate);
  on("/time", handleTime);
  on("/setRelay", handleRelay);
  on("/relayTime", handleRelayTime);
  on("/sensors", handleSensors);
  
  enableCORS(true);
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

/*static*/ void WioGreenhouseServer::handleDate()
{
    _singleton->getDate();
}

/*static*/ void WioGreenhouseServer::handleTime()
{
    _singleton->getTime();
}

/*static*/ void WioGreenhouseServer::handleRelay()
{
    _singleton->setRelay();
}

/*static*/ void WioGreenhouseServer::handleRelayTime()
{
  _singleton->setRelayTime();
}

/*static*/ void WioGreenhouseServer::handleSensors()
{
  _singleton->getSensors();
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
      String("{\n  \"version\":\"")      + _app.getVersionStr() + "\"," +
      String( "\n  \"uptime\":\"")       + _app.getBootupTime() + "\"," +
      String( "\n  \"bootReason\":\"")   + _app.getBootReasonString() + "\"," +
      String( "\n  \"time\":\"")         + _app.getTime() + "\"," +
      String( "\n  \"IP\": \"")          + _app.getIP() + "\"," +
      String( "\n  \"MAC\": \"")         + _app.getMAC() + "\"," +
      String(" \n  \"wifiConnected\": ") + (_app.isWifiConnected() ? "true, " : "false, ") +
      String( "\n  \"mqttConnected\": ") + (_app.isMqttConnected() ? "true, " : "false, ") +
      String( "\n  \"sensorsStatus\": ") + String(_app.getSensorsStatus()) +  "," +
      String( "\n  \"relays\": {") +
      String( "\n    \"relay1\": {") +
      String( "\n      \"on\": ")        + (_app.isRelayOn(0) ? "true, " : "false, ") +
      String( "\n      \"override\":" )  + (_app.getRelayOverride(0) ? "true, " : "false, ") +
      String( "\n    },") +
      String( "\n    \"relay2\": {") +
      String( "\n      \"on\": ")        + (_app.isRelayOn(1) ? "true, " : "false, ") +
      String( "\n      \"override\": ")  + (_app.getRelayOverride(1) ? "true, " : "false, ") +
      String( "\n    }") +
      String( "\n  },") +
      String( "\n}"));
  }
}

void WioGreenhouseServer::getDate()
{
  if (method() == HTTP_GET)
  {
    send(200, "application/json", String(_app.getDate()) + String("\n"));
  }
}

void WioGreenhouseServer::getTime()
{
  if (method() == HTTP_GET)
  {
    send(200, "application/json", String(_app.getTime()) + String("\n"));
  }
}

void WioGreenhouseServer::setRelay()
{
  if (method() != HTTP_POST)
  {
    send(405, "text/plain", "Method Not Allowed");
    Serial.println(method()==HTTP_POST ? "POST" : method()==HTTP_PUT ? "PUT" : "GET");
  }
  
  if (argName(0) == "on")
  {
    unsigned long delayValue = 0; // zero means don't change
    if (hasArg("delay"))
    {
      delayValue = arg("delay").toInt();
      if (delayValue < 5000) delayValue = 0; // Ignore invalid values
    }

    int relayIndex = 0;
    if (hasArg("relayIndex"))
    {
      relayIndex = arg("relayIndex").toInt();
      if (relayIndex < 0 || relayIndex > 1)
      {
        send(400, "text/plain", String("Invalid relay index\n"));
        return;
      }
    }
    
    if (arg(0) == "yes")
    {
      _app.setRelay(relayIndex, true, delayValue);
      send(200, "text/plain", String("Relay turned on for ") + String(delayValue) + "ms.\n");
    }
    else if (arg(0) == "no")
    {
      _app.setRelay(relayIndex, false, delayValue);
      send(200, "text/plain", String("Relay turned off for ") + String(delayValue) + "ms.\n");
    }
    else
    {
      send(400, "text/plain", String("Relay status not specified or invalid\n"));
    }
  }
}

void WioGreenhouseServer::setRelayTime()
{

}

void WioGreenhouseServer::getSensors()
{
  if (method() == HTTP_GET)
  {
    Serial.println("HTTP server: request for /sensors.");
    send(200, "application/json",
      String("{\n  \"temperature\": ") + _app.getDevices().getTemp() + ", " +
      String( "\n  \"humidity\": ")    + _app.getDevices().getHum() + ", " +
      String( "\n  \"lux\": ")         + _app.getDevices().getLux() + "\n}\n");
  }
}
