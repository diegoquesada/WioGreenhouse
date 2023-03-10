/**
 * WioGreenhouseDeviceMgr.h
 * Declaration of WioGreenhouseDeviceMgr class.
 * (c) 2023 Diego Quesada
*/
#pragma once

class WioGreenhouseDeviceMgr
{
public:
    WioGreenhouseDeviceMgr();

    unsigned char updateSensors();

private:
    const unsigned long UPDATE_INTERVAL = 30000; /// How often we should update sensors in ms.
    unsigned long _lastUpdateTime = 0; /// Last time we updated sensors
};
