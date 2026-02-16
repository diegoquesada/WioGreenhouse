/**
 * WioGreenhouseDeviceMgr.ino
 * Definition of WioGreenhouseDeviceMgr class.
 * (c) 2025 Diego Quesada
*/

#include "WioGreenhouseDeviceMgr.h"
#include "WioGreenhouseApp.h"
//#include <Digital_Light_TSL2561.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

const int dhtPin = 14;

Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_LOW, 12345);

WioGreenhouseDeviceMgr::WioGreenhouseDeviceMgr(unsigned long updateInterval) :
    _updateTimer(updateInterval),
    _dht(dhtPin, DHT11)
{

}

void WioGreenhouseDeviceMgr::setup()
{
  Wire.begin();
  _dht.begin();

  if (!tsl.begin())
  {
    Serial.println("TSL2561 not found.");
  }
  else
  {
    tsl.setGain(TSL2561_GAIN_1X);
    tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);
    Serial.println("Found TSL2561 sensor.");
  }
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
uint8_t WioGreenhouseDeviceMgr::updateSensors()
{
  if (_updateTimer.IsItTime())
  {
    WioGreenhouseApp::getApp().printTime();

    if (!_dht.readTempAndHumidity(_temp_hum_val))
    {
      _sensorsStatus = SENSORS_STATUS_TEMPHUMOK;

      sensors_event_t event;
      tsl.getEvent(&event);
      if (event.light)
      {
        _lux = event.light;
        _sensorsStatus |= SENSORS_STATUS_LIGHTOK;
        //Serial.println("Lux sensor OK");
      }
      else
      {
        _lux = 0.0;
      }
      
      WioGreenhouseApp::getApp().printTime();
      Serial.print("Humidity: ");
      Serial.print(_temp_hum_val[0]);
      Serial.print(" %\tTemperature: ");
      Serial.print(_temp_hum_val[1]);
      Serial.print(" *C\tLux: ");
      Serial.println(_lux);
    }
    else
    {
      Serial.println("Failed to get temperature and humidity.");
      _sensorsStatus = SENSORS_STATUS_ERROR;
    }
  
    _updateTimer.Reset();

    return (_sensorsStatus != SENSORS_STATUS_ERROR); // Return success=1 or failure=0
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