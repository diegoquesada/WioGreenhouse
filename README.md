# WioGreenhouse

A WioLink ESP8266 project to monitor and control a small greenhouse.
Copyright 2022 Diego Quesada

## Features
- Controls grow lights via SPDT relay
- Measures temp, humidity and lux inside greenhouse
- Logs light, temp and humidity to MQTT broker

## Future improvements
- Replace the DHT11 temp/humidity sensor with BME680
- Capture critical exceptions into flash for later analysis
- Add REST APIs to return sensor values
- Add REST APIs to set parameters such as time on/off
- Have web server provide a simple UI to access API
- Log sensor values to an Azure or AWS MQTT endpoint instead of local Mosquitto
- Refactor code into multiple files
- Implement basic HomeKit support

## References
https://github.com/esp8266/Arduino
https://github.com/adafruit/DHT-sensor-library
https://github.com/Seeed-Studio/Grove_Digital_Light_Sensor
https://github.com/me-no-dev/EspExceptionDecoder
https://github.com/arduino-libraries/NTPClient
