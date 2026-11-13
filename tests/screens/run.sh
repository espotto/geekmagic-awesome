#!/bin/sh
# Desktop test of the firmware's own screens (ScreenView). An optional argument
# is a PPM path to write the sample screens to.
set -eu
cd "$(dirname "$0")/../.."
c++ -std=c++11 -Wall -Wextra -Werror -g -Isrc -Isrc/features/theme \
  tests/screens/test_screens.cpp src/ScreenView.cpp src/SysText.cpp src/QrCode.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-screen-tests
/tmp/geekmagic-screen-tests "$@"
