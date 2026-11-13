#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
THEME_JSON_INCLUDE=${THEME_JSON_INCLUDE:-.pio/libdeps/geekmagic/ArduinoJson/src}
c++ -std=c++11 -Wall -Wextra -Werror -g -Isrc/features/theme -I"$THEME_JSON_INCLUDE" tests/theme/test_engine.cpp src/features/theme/ThemeEngine.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-theme-tests
/tmp/geekmagic-theme-tests
c++ -std=c++11 -Wall -Wextra -Werror -g -Isrc/features/theme -I"$THEME_JSON_INCLUDE" tests/theme/test_builtin.cpp src/features/theme/ThemeEngine.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-theme-builtin-tests
/tmp/geekmagic-theme-builtin-tests
c++ -std=c++11 -Wall -Wextra -Werror -g -Isrc/features/theme -I"$THEME_JSON_INCLUDE" tests/theme/test_text_tokens.cpp src/features/theme/ThemeEngine.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-theme-text-tokens-tests
/tmp/geekmagic-theme-text-tokens-tests
c++ -std=c++11 -Wall -Wextra -Werror -g -pthread -Isrc/features/theme -I"$THEME_JSON_INCLUDE" tests/theme/test_data.cpp src/features/theme/ThemeEngine.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-theme-data-tests
/tmp/geekmagic-theme-data-tests
c++ -std=c++11 -Wall -Wextra -Werror -g -Isrc/features/theme -I"$THEME_JSON_INCLUDE" tests/theme/test_package.cpp src/features/theme/ThemePackage.cpp src/features/theme/ThemeEngine.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-theme-package-tests
/tmp/geekmagic-theme-package-tests
c++ -std=c++11 -Wall -Wextra -Werror -g -Isrc/features/theme -I"$THEME_JSON_INCLUDE" tests/theme/test_render.cpp src/features/theme/ThemeEngine.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-theme-render-tests
/tmp/geekmagic-theme-render-tests
c++ -std=c++11 -Wall -Wextra -Werror -g -Isrc/features/theme -I"$THEME_JSON_INCLUDE" tests/theme/test_validation.cpp src/features/theme/ThemeEngine.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-theme-validation-tests
c++ -std=c++11 -Wall -Wextra -Werror -g -Isrc/features/theme -I"$THEME_JSON_INCLUDE" tests/theme/test_language.cpp src/features/theme/ThemeEngine.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-theme-language-tests
/tmp/geekmagic-theme-language-tests
c++ -std=c++11 -Wall -Wextra -Werror -g -Isrc/features/theme -I"$THEME_JSON_INCLUDE" tests/theme/test_font.cpp src/features/theme/ThemeEngine.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-theme-font-tests
/tmp/geekmagic-theme-font-tests
/tmp/geekmagic-theme-validation-tests
c++ -std=c++11 -Wall -Wextra -Werror -g -Isrc/features/theme -I"$THEME_JSON_INCLUDE" -I".pio/libdeps/geekmagic/GFX Library for Arduino/src" tests/theme/check_example.cpp src/features/theme/ThemePackage.cpp src/features/theme/ThemeEngine.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-theme-example-check
/tmp/geekmagic-theme-example-check tests/theme/fixtures/pixel-room.stheme
/tmp/geekmagic-theme-example-check tests/theme/fixtures/terminal-ops.stheme
c++ -std=c++11 -Wall -Wextra -Werror -g -Isrc/features/theme -I"$THEME_JSON_INCLUDE" tests/theme/test_catalog.cpp src/features/theme/ThemePackage.cpp src/features/theme/ThemeEngine.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-theme-catalog-tests
/tmp/geekmagic-theme-catalog-tests
# 32-bit, so allocation sizes match the ESP8266; the wraps let the test count
# ArduinoJson's malloc calls as well as operator new. Linux only (g++-multilib,
# malloc_usable_size).
[ "$(uname)" = Linux ] || exit 0
c++ -m32 -std=c++11 -Wall -Wextra -Werror -g -Isrc/features/theme -I"$THEME_JSON_INCLUDE" -Wl,--wrap=malloc,--wrap=free,--wrap=realloc tests/theme/test_memory.cpp src/features/theme/ThemePackage.cpp src/features/theme/ThemeEngine.cpp src/features/theme/SystemFont.cpp -o /tmp/geekmagic-theme-memory-tests
/tmp/geekmagic-theme-memory-tests
