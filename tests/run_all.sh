#!/bin/sh
# Every host test of the firmware and the web page, from the project root.
# Needs a C++ compiler, node, Python 3 with Pillow, and the libraries PlatformIO
# fetched for the build (pio pkg install). The 32-bit memory tests run only on
# Linux (they need g++-multilib and malloc_usable_size).
set -eu
# PYTHON: a Python with Pillow for the preview test (default python3).
cd "$(dirname "$0")/.."
export THEME_JSON_INCLUDE="${THEME_JSON_INCLUDE:-.pio/libdeps/geekmagic/ArduinoJson/src}"
LINUX=0; [ "$(uname)" = Linux ] && LINUX=1
J="$THEME_JSON_INCLUDE"
# KEEP_GOING=1 runs every suite and fails at the end (CI), otherwise the first
# failure stops the run.
FAILED=""
run() { printf '%-26s ' "$1"; n="$1"; shift; "$@" >/tmp/geekmagic-test.log 2>&1 && echo ok || { echo FAIL; tail -20 /tmp/geekmagic-test.log; FAILED="$FAILED $n"; [ -n "${KEEP_GOING:-}" ] || exit 1; }; }

run webui.h python3 tools/gzip_webui.py --check
run default-faces python3 tools/gen_default_faces.py --check
run pwa-assets python3 tools/gen_pwa.py --check
for t in screens notify calendar reminders apps mode auth sun; do run "$t" sh "tests/$t/run.sh"; done
run status sh -c "c++ -std=c++11 -Wall -Wextra -Werror -Isrc -I$J tests/status/test_status.cpp -o /tmp/geekmagic-status && /tmp/geekmagic-status"
if [ $LINUX = 1 ]; then run status-memory sh tests/status/run.sh; fi
run storage sh -c "c++ -std=c++11 -Wall -Wextra -Werror -Isrc -I$J tests/test_storage.cpp -o /tmp/geekmagic-storage && /tmp/geekmagic-storage"
run webguard sh -c "c++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/test_webguard.cpp -o /tmp/geekmagic-webguard && /tmp/geekmagic-webguard"
run crash-record sh -c "c++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/test_crash_record.cpp -o /tmp/geekmagic-crash && /tmp/geekmagic-crash"
run theme sh tests/theme/run.sh
for t in tests/*/test_webui*.js tests/*/test_wiring.js; do run "$(basename "$(dirname "$t")")/$(basename "$t" .js)" node "$t"; done
for t in test_webui_preview test_pack test_cli; do run "theme/$t" ${PYTHON:-python3} -m unittest "tests/theme/$t.py"; done
if [ -n "$FAILED" ]; then echo "failed:$FAILED"; exit 1; fi
echo "all tests passed"
