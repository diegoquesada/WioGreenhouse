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
    bool areSensorsOK() const { return _sensorsOK; }
    String getTime() const { return String(_timeClient.getFormattedTime()); }

private:
    void initWifi();
    bool connectMQTT();
    bool initHTTPServer();

    static void mqttCallback(char* topic, byte* payload, unsigned int length);

private:
    static WioGreenhouseApp *_singleton;

    bool _wifiConnected = false;
    bool _mqttConnected = false;
    bool _sensorsOK = false;
    bool _relayState = false;

    DHT _dht; /// Grove digital humidity and temp sensor
    float _temp_hum_val[2] = {0}; /// Last measured temp and humidity from DHT sensor -- 0: humidity, 1: temp
    long _lux = 0; /// Last measured lux value

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
