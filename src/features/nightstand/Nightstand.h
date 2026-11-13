// Nightstand.h — the night clock over every app (Settings: clock.nightstand):
// while it is night (the night schedule, or the sun schedule's night), main.cpp
// draws this instead of the app. It repaints once a minute.
#pragma once
#include "Settings.h"

bool nightstandWanted(const Settings& s);
// Draws when the minute (or `force`) says so.
void nightstandService(const Settings& s, bool force);
