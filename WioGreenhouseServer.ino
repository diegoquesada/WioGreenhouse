#include "WioGreenHouseServer.h"
#include "WioGreenHouseApp.h"

bool WioGreenhouseServer::init()
{
  on("/", handleRoot);
  on("/status", handleStatus);
  on("/time", handleTime);
  on("/relay", handleRelay);
  
  begin();
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
    return _singleton->app->getTime();
}

/*static*/ void WioGreenhouseServer::handleRelay()
{

}

String WioGreenhouseServer::getHomepage()
{
    Serial.println("HTTP server: request for root.");
    send(200, "text/html",
        "<html><head><title>GrowLights</title></head><body>" + _app->getVersionStr() + "</body></html>");
}

String WioGreenhouseServer::getStatus()
{
  if (method() == HTTP_GET)
  {
    Serial.println("HTTP server: request for /status.");
    send(200, "application/json",
      String("{ \"wifiConnected\": ") + String(app->isWifiConnected() ? "true, " : "false, ") +
      String("\"mqttConnected\":")    + String(_app->isMqttConnected() ? "true, " : "false, ") +
      String("\"sensorsOK\":")        + String(_app->areSensorsOK() ? "true }" : "false }"));
  }
}