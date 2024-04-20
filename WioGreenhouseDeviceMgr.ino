/**
 * WioGreenhouseDeviceMgr.ino
 * Definition of WioGreenhouseDeviceMgr class.
 * (c) 2023 Diego Quesada
*/

#include "WioGreenhouseDeviceMgr.h"
#include "WioGreenHouseApp.h"
#include <Digital_Light_TSL2561.h>

const int dhtPin = 14;

WioGreenhouseDeviceMgr::WioGreenhouseDeviceMgr() :
    _updateTimer(DEFAULT_UPDATE_INTERVAL),
    _dht(dhtPin, DHT11)
{

}

void WioGreenhouseDeviceMgr::setup()
{
  _dht.begin();

  Wire.begin();
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
    WioGreenhouseApp::getApp().printTime();

    if (!_dht.readTempAndHumidity(_temp_hum_val))
    {
      _lux = TSL2561.readVisibleLux();
      
      Serial.print("Humidity: ");
      Serial.print(_temp_hum_val[0]);
      Serial.print(" %\tTemperature: ");
      Serial.print(_temp_hum_val[1]);
      Serial.print(" *C\tLux: ");
      Serial.println(_lux);

      _updateTimer.Dump();

      _sensorsOK = true;
    }
    else
    {
      Serial.println("Failed to get temperature and humidity.");
      _sensorsOK = false;
    }
  
    _updateTimer.Reset();

    return _sensorsOK; // Return success=1 or failure=9
  }
  else
  {
    return 2; // A return value of 2 means we didn't update the sensors
  }
}

void WioGreenhouseDeviceMgr::setUpdateInterval(unsigned long interval)
{
  // Minimum: 30 seconds, maximum: 1 hour
  if (interval >= 30 * 1000 && interval <= 60 * 60 * 1000)
  {
    _updateTimer.setDelay(interval);
    _updateTimer.Reset();
  }
}