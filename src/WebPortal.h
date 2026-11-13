// WebPortal.h — HTTP config UI, REST API, and OTA endpoint
#pragma once
#include <Arduino.h>
#include "Settings.h"

void webPortalBegin(Settings& settings);
void webPortalLoop();
bool webPortalRebootDue();   // main polls this and calls ESP.restart()
bool webPortalRebootPending();   // a restart is scheduled: the screen is left as it is
