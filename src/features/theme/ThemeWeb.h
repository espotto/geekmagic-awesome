#pragma once
#include "config.h"
#include "Platform.h"
#include "Settings.h"
void themeWebBegin(WebServerClass& server, Settings& settings, bool (*requireAuth)(), bool (*requestAllowed)());
