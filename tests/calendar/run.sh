#!/bin/sh
# Desktop test of the Calendar app's month screen (CalendarView). An optional
# argument is a PPM path to write sample months to.
set -eu
cd "$(dirname "$0")/../.."
c++ -std=c++11 -Wall -Wextra -Werror -g -Isrc/features/calendar -Isrc/features/theme \
  tests/calendar/test_view.cpp src/features/calendar/CalendarView.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-calendar-tests
/tmp/geekmagic-calendar-tests "$@"
