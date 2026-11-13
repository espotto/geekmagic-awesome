#!/bin/sh
# Desktop test of the notification screen's layout and rendering (NotifyView).
# An optional argument is a PPM path to write sample screens to.
set -eu
cd "$(dirname "$0")/../.."
THEME_JSON_INCLUDE=${THEME_JSON_INCLUDE:-.pio/libdeps/geekmagic/ArduinoJson/src}
c++ -std=c++11 -Wall -Wextra -Werror -g -Isrc/features/notify -Isrc/features/theme -I"$THEME_JSON_INCLUDE" \
  tests/notify/test_view.cpp src/features/notify/NotifyView.cpp src/features/theme/ThemeEngine.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-notify-view-tests
/tmp/geekmagic-notify-view-tests "$@"
