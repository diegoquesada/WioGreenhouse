/**
 * WioLink ESP8266 project to monitor and control a greenhouse.
 * Copyright 2022 Diego Quesada
 */

#include "WioGreenhouseApp.h"


//--------------------------------------------------------------------------------
// Initialization

WioGreenhouseApp *app = nullptr;

void setup()
{
  app = new WioGreenhouseApp;
  app->setup();
}

void loop()
{
  app->loop();
}

