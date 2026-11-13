#!/bin/sh
# Desktop test of the Reminders app: the list's rules (RemindersList) and its
# screen (RemindersView). An optional argument is a PPM path for sample screens.
set -eu
cd "$(dirname "$0")/../.."
c++ -std=c++11 -Wall -Wextra -Werror -g -Isrc/features/reminders -Isrc/features/theme \
  tests/reminders/test_reminders.cpp src/features/reminders/RemindersView.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-reminders-tests
/tmp/geekmagic-reminders-tests "$@"
