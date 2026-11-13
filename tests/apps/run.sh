#!/bin/sh
# Desktop tests of the firmware's own apps: Weather (WeatherView), Timer
# (TimerView) and the night clock (NightstandView). An optional argument is a
# path prefix for sample screens: <prefix>-weather.ppm and <prefix>-timer.ppm.
set -eu
cd "$(dirname "$0")/../.."
FLAGS="-std=c++11 -Wall -Wextra -Werror -g -Isrc/features -Isrc/features/theme -Itests/apps"
c++ $FLAGS tests/apps/test_weather.cpp src/features/weather/WeatherView.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-app-weather
c++ $FLAGS tests/apps/test_timer.cpp src/features/timer/TimerView.cpp src/features/nightstand/NightstandView.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-app-timer
if [ $# -gt 0 ]; then
  /tmp/geekmagic-app-weather "$1-weather.ppm"
  /tmp/geekmagic-app-timer "$1-timer.ppm"
else
  /tmp/geekmagic-app-weather
  /tmp/geekmagic-app-timer
fi
