/**
 * WioGreenhouseDeviceMgr.h
 * Declaration of WioGreenhouseDeviceMgr class.
 * (c) 2023 Diego Quesada
*/
#pragma once

#include "TimerCounter.h"
class WioGreenhouseDeviceMgr
{
public:
    WioGreenhouseDeviceMgr();

    void setup();
    unsigned char updateSensors();
    unsigned long getUpdateInterval() const { return _updateTimer.getDelay(); }
    void setUpdateInterval(unsigned long interval); /// Changes the frequency of sensor updates

    bool areSensorsOK() const { return _sensorsOK; }

    float getTemp() const { return _temp_hum_val[1]; }
    float getHum() const { return _temp_hum_val[0]; }
    long getLux() const { return _lux; }

private:
    const unsigned long DEFAULT_UPDATE_INTERVAL = 5 * 60 * 1000; /// Default sensor update frequency - 5 min

    TimerCounter _updateTimer;
    bool _sensorsOK = false;

    DHT _dht; /// Grove digital humidity and temp sensor
    float _temp_hum_val[2] = {0}; /// Last measured temp and humidity from DHT sensor -- 0: humidity, 1: temp
    long _lux = 0; /// Last measured lux value
};
