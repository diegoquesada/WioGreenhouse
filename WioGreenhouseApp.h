#pragma once

#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <DHT.h>
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
    uint32_t getSerialNumber() const;
    String getIP() const { return WiFi.localIP().toString(); }
    String getMAC() const { return WiFi.macAddress(); }
    bool isRelayOn() const { return _relayState; }
    bool getRelayOverride() const { return _relayOverride; }
    String getTime() const { return String(_timeClient.getFormattedTime()); }
    bool areSensorsOK() const { return _devices.areSensorsOK(); }
    String getBootupTime();
    void printTime(); /// Prints the current internal time since boot on the serial

    void setRelay(bool on, unsigned long delay);

    WioGreenhouseDeviceMgr& getDevices() { return _devices; }
    static WioGreenhouseApp& getApp() { return *_singleton; }

private:
    void initWifi();
    bool connectMQTT();
    bool initHTTPServer();
    bool pushUpdate();
    void updateRelay();

    void getSensorsJson(char *jsonOut) const;

    static void mqttCallback(char* topic, byte* payload, unsigned int length);

private:
    static WioGreenhouseApp *_singleton;

    bool _wifiConnected = false;
    bool _mqttConnected = false;
    bool _relayState = false;

    unsigned long _bootupTime = 0; /// Set to 0 by default, will be initialized in loop()

    const unsigned long RELAY_OVERRIDE = 60 * 60 * 1000; /// If relay overriden via API, this is how long the override will hold.
    unsigned char _relayOverride = 0; /// 0 if not overridden (driven by time), 1 is override on, 2 is override off
    TimerCounter _relayTimer = 0; /// When the override was set
    unsigned char relay1OnTime = 6;
    unsigned char relay1OffTime = 20;

    WiFiClient _wifiClient;

    PubSubClient _pubSubClient;
    const unsigned long PUBSUB_INTERVAL = 5 * 60 * 1000; /// If PubSub not connected, frequency of retries in ms.
    TimerCounter _pubSubTimer;

    WiFiUDP _ntpUDP;
    const int timeOffset = -4 * 60 * 60; // 5 hours offset (in seconds) during DST
    NTPClient _timeClient;

    WioGreenhouseDeviceMgr _devices;

    WioGreenhouseServer _webServer;
};
