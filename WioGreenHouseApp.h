#pragma once

#include <WiFiClient.h>
#include <WiFiUdp.h>
#include "DHT.h"
#include <PubSubClient.h>
#include <NTPClient.h>
#include "WioGreenhouseDeviceMgr.h"
#include "WioGreenhouseServer.h"

class WioGreenhouseApp {
public:
    WioGreenhouseApp();

    void setup();
    void loop();

    String getVersionStr() const;
    bool isWifiConnected() const { return _wifiConnected; }
    bool isMqttConnected() const { return _mqttConnected; }
    String getTime() const { return String(_timeClient.getFormattedTime()); }

private:
    void initWifi();
    bool connectMQTT();
    bool initHTTPServer();
    bool pushUpdate();
    void updateRelay();

    static void mqttCallback(char* topic, byte* payload, unsigned int length);

private:
    static WioGreenhouseApp *_singleton;

    bool _wifiConnected = false;
    bool _mqttConnected = false;
    bool _relayState = false;

    const unsigned long RELAY_OVERRIDE = 60*60*60; /// If relay overriden via API, this is how long the override will hold.
    unsigned short _relayOverride = 0; /// 0 if not overridden (driven by time), 1 is override on, 2 is override off
    unsigned long _relayOverrideTime = 0; /// When the override was set

    WiFiClient _wifiClient;
    PubSubClient _pubSubClient;

    WiFiUDP _ntpUDP;
    const int timeOffset = -4 * 60 * 60; // 5 hours offset (in seconds) during DST
    NTPClient _timeClient;

    WioGreenhouseDeviceMgr _devices;

    WioGreenhouseServer _webServer;
};
