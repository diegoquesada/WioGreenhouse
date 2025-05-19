#pragma once

#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <DHT.h>
#include <PubSubClient.h>
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
    bool isRelayOn(uint8_t relayIndex) const { return _relayState[relayIndex]; }
    bool getRelayOverride(uint8_t relayIndex) const { return _relayOverride[relayIndex]; }
    uint8_t getSensorsStatus() const { return _devices.getSensorsStatus(); }

    String getBootReasonString() const;
    String getDate() const;
    String getTime() const; /// Returns the current time in HH:MM:SS format
    String getBootupTime(); /// Returns the time elapsed since last bootup
    void printTime(); /// Prints the current internal time since boot on the serial

    void setRelay(uint8_t relayIndex, bool on, unsigned long delay);

    WioGreenhouseDeviceMgr& getDevices() { return _devices; }
    static WioGreenhouseApp& getApp() { return *_singleton; }

private:
    void initWifi();
    bool connectMQTT();
    bool initHTTPServer();
    bool pushUpdate(const char *topic, const char *json, bool retained = false);
    bool updateRelay(uint8_t relayIndex);

    void getSensorsJson(char *jsonOut) const;

    static void mqttCallback(char* topic, byte* payload, unsigned int length);

private:
    static WioGreenhouseApp *_singleton;

    bool _wifiConnected = false;
    bool _mqttConnected = false;
    bool _powerSavingEnabled = false; /// If true, the device will go to sleep after taking and sending measurements.
    bool _relayState[2] = { false, false };

    const unsigned long RELAY_OVERRIDE = 60 * 60 * 1000; /// If relay overriden via API, this is how long the override will hold.
    unsigned char _relayOverride[2] = { 0, 0 }; /// 0 if not overridden (driven by time), 1 is override on, 2 is override off
    TimerCounter _relayTimer[2] = { RELAY_OVERRIDE, RELAY_OVERRIDE };
    const int8_t RELAY_ALWAYSON = -1;
    int8_t relayOnTime[2] = { RELAY_ALWAYSON, 6 }; /// Hour of the day when the relay should be on; RELAY_ALWAYSON for always on.
    int8_t relayOffTime[2] = { RELAY_ALWAYSON, 20 }; /// Hour of the day when the relay should be off.

    WiFiClient _wifiClient;

    PubSubClient _pubSubClient;
    const unsigned long PUBSUB_INTERVAL = 5 * 60 * 1000; /// If PubSub not connected, frequency of retries in ms.
    TimerCounter _pubSubTimer;

    const unsigned long DEFAULT_UPDATE_INTERVAL = 5 * 60 * 1000; /// Default sensor update frequency - 5 min
    WioGreenhouseDeviceMgr _devices;

    WioGreenhouseServer _webServer;
};
