#!/bin/sh
# Desktop tests of the firmware's own apps: Weather (WeatherView), Timer
# (TimerView), Pomodoro (PomodoroCore, PomodoroView),
# Exchange Rates (CurrencyJson, CurrencyView) and the night clock (NightstandView). An optional argument is a
# path prefix for sample screens: <prefix>-weather.ppm and <prefix>-timer.ppm.
set -eu
cd "$(dirname "$0")/../.."
J="${THEME_JSON_INCLUDE:-.pio/libdeps/geekmagic/ArduinoJson/src}"
FLAGS="-std=c++11 -Wall -Wextra -Werror -g -Isrc/features -Isrc/features/theme -Itests/apps"
c++ $FLAGS tests/apps/test_weather.cpp src/features/weather/WeatherView.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-app-weather
c++ $FLAGS tests/apps/test_timer.cpp src/features/timer/TimerView.cpp src/features/nightstand/NightstandView.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-app-timer
c++ $FLAGS -I"$J" tests/apps/test_currency.cpp src/features/currency/CurrencyView.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-app-currency
c++ $FLAGS tests/apps/test_pomodoro.cpp src/features/pomodoro/PomodoroView.cpp src/features/timer/TimerView.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-app-pomodoro
if [ $# -gt 0 ]; then
  /tmp/geekmagic-app-weather "$1-weather.ppm"
  /tmp/geekmagic-app-timer "$1-timer.ppm"
  /tmp/geekmagic-app-pomodoro "$1-pomodoro.ppm"
  /tmp/geekmagic-app-currency "$1-currency.ppm"
else
  /tmp/geekmagic-app-weather
  /tmp/geekmagic-app-timer
  /tmp/geekmagic-app-pomodoro
  /tmp/geekmagic-app-currency
fi
