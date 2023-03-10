/**
 * WioGreenhouseDeviceMgr.ino
 * Definition of WioGreenhouseDeviceMgr class.
 * (c) 2023 Diego Quesada
*/

#include "WioGreenhouseDeviceMgr.h"

WioGreenhouseDeviceMgr::WioGreenhouseDeviceMgr()
{

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

  if (lastUpdateTime == 0) // special case: first time
  {
    needsUpdate = true;
  }
  else if (currentTime < lastUpdateTime) // we wrapped
  { 
    needsUpdate = (ULONG_MAX - lastUpdateTime + 1 + currentTime) > UPDATE_INTERVAL;
  }
  else
  {
    needsUpdate = (currentTime - lastUpdateTime) > UPDATE_INTERVAL;
  }

  if (needsUpdate)
  {
    if (!dht.readTempAndHumidity(temp_hum_val))
    {
      lux = TSL2561.readVisibleLux();
      
      Serial.print("Humidity: ");
      Serial.print(temp_hum_val[0]);
      Serial.print(" %\tTemperature: ");
      Serial.print(temp_hum_val[1]);
      Serial.print(" *C\tLux: ");
      Serial.println(lux);
  
      sensorsOK = true;
    }
    else
    {
      Serial.println("Failed to get temperature and humidity.");
      sensorsOK = false;
    }
  
    lastUpdateTime = millis();
    if (lastUpdateTime == 0) lastUpdateTime++; // zero is a special case.

    return sensorsOK;
  }
  else
  {
    return 2;
  }
}

