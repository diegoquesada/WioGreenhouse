/**
 * WioGreenhouseDeviceMgr.h
 * Declaration of WioGreenhouseDeviceMgr class.
 * (c) 2023 Diego Quesada
*/
#pragma once

#include "TimerCounter.h"
const uint8_t SENSORS_STATUS_ERROR = 0x0;
const uint8_t SENSORS_STATUS_TEMPHUMOK = 0x01;
const uint8_t SENSORS_STATUS_LIGHTOK   = 0x02;
const uint8_t SENSORS_OK = SENSORS_STATUS_TEMPHUMOK | SENSORS_STATUS_LIGHTOK;

class WioGreenhouseDeviceMgr
{
public:
    WioGreenhouseDeviceMgr(unsigned long updateInterval);

    void setup();
    uint8_t updateSensors();
    void setUpdateInterval(unsigned long interval); /// Changes the frequency of sensor updates

    uint8_t getSensorsStatus() const { return _sensorsStatus; }

    float getTemp() const { return _temp_hum_val[1]; }
    float getHum() const { return _temp_hum_val[0]; }
    long getLux() const { return _lux; }

private:
    TimerCounter _updateTimer;
    uint8_t _sensorsStatus = false;

    DHT _dht; /// Grove digital humidity and temp sensor
    float _temp_hum_val[2] = {0}; /// Last measured temp and humidity from DHT sensor -- 0: humidity, 1: temp
    float _lux = 0; /// Last measured lux value
};
