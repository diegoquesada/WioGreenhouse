/**
 * WioGreenhouseDeviceMgr.ino
 * Definition of WioGreenhouseDeviceMgr class.
 * (c) 2023 Diego Quesada
*/

#include "WioGreenhouseDeviceMgr.h"
#include <Digital_Light_TSL2561.h>

const int dhtPin = 14;

WioGreenhouseDeviceMgr::WioGreenhouseDeviceMgr() :
    _dht(dhtPin, DHT11)
{

}

void WioGreenhouseDeviceMgr::setup()
{
  Wire.begin();
  _dht.begin();

  TSL2561.init();
}

//--------------------------------------------------------------------------------
// Sensor updates

/**
 * Takes readings from temp, humidity and lux sensors.
 * Updates sensorsOK global variable to indicate whether all sensors are working properly.
 * @return 0 if at least one sensor could not be retrieved successfully
 *         1 if all sensors were retreaved successfully
 *         2 if it is not yet time to update
 */
unsigned char WioGreenhouseDeviceMgr::updateSensors()
{
  unsigned long currentTime = millis();
  bool needsUpdate = false;

  if (_lastUpdateTime == 0) // special case: first time
  {
    needsUpdate = true;
  }
  else if (currentTime < _lastUpdateTime) // we wrapped
  { 
    needsUpdate = (ULONG_MAX - _lastUpdateTime + 1 + currentTime) > UPDATE_INTERVAL;
  }
  else
  {
    needsUpdate = (currentTime - _lastUpdateTime) > UPDATE_INTERVAL;
  }

  if (needsUpdate)
  {
    if (!_dht.readTempAndHumidity(temp_hum_val))
    {
      lux = TSL2561.readVisibleLux();
      
      Serial.print("Humidity: ");
      Serial.print(_temp_hum_val[0]);
      Serial.print(" %\tTemperature: ");
      Serial.print(_temp_hum_val[1]);
      Serial.print(" *C\tLux: ");
      Serial.println(_lux);
  
      _sensorsOK = true;
    }
    else
    {
      Serial.println("Failed to get temperature and humidity.");
      _sensorsOK = false;
    }
  
    lastUpdateTime = millis();
    if (lastUpdateTime == 0) lastUpdateTime++; // zero is a special case.

    return _sensorsOK;
  }
  else
  {
    return 2;
  }
}

