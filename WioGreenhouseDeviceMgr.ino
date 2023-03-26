/**
 * WioGreenhouseDeviceMgr.ino
 * Definition of WioGreenhouseDeviceMgr class.
 * (c) 2023 Diego Quesada
*/

#include "WioGreenhouseDeviceMgr.h"
#include <Digital_Light_TSL2561.h>

const int dhtPin = 14;

WioGreenhouseDeviceMgr::WioGreenhouseDeviceMgr() :
    _updateTimer(UPDATE_INTERVAL),
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
 *         1 if all sensors were retrieved successfully
 *         2 if it is not yet time to update
 */
unsigned char WioGreenhouseDeviceMgr::updateSensors()
{
  if (_updateTimer.IsItTime())
  {
    if (!_dht.readTempAndHumidity(_temp_hum_val))
    {
      _lux = TSL2561.readVisibleLux();
      
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
  
    _updateTimer.Reset();

    return _sensorsOK;
  }
  else
  {
    return 2;
  }
}

