/**
 * WioLink ESP8266 project to monitor and control a greenhouse.
 * Copyright 2022 Diego Quesada
 */

#include "WioGreenhouseApp.h"


//--------------------------------------------------------------------------------
// Initialization

WioGreenhouseApp app;

void setup()
{
  app.setup();
}

void loop()
{
  app.loop();
}

