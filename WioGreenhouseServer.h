#pragma once

#include <ESP8266WebServer.h>

class WioGreenhouseApp;

class WioGreenhouseServer : public ESP8266WebServer
{
public:
    WioGreenhouseServer(WioGreenhouseApp *app) :
        ESP8266WebServer(80), _app(app) { _singleton = this; }

    bool init();

private:
    static void handleRoot();
    static void handleStatus();
    static void handleTime();
    static void handleRelay();

    String getHomepage();
    String getStatus();

private:
    WioGreenhouseApp *_app;
    static WioGreenhouseServer *_singleton;
};
