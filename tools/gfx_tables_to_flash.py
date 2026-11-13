# gfx_tables_to_flash.py — PlatformIO pre-build script for the ESP8266 images.
#
# The GFX library declares its JPEG colour tables (YCbCr2RGB.h: Y2I16, CB2*,
# CR2*, CLIP*) as plain `static const` arrays. On the ESP8266 that puts about
# 6.8 KB of them in RAM, out of the heap, although the firmware never decodes
# a JPEG: only Arduino_DataBus::writeYCbCrPixels() reads them, kept alive by
# its vtable. This adds PROGMEM to those declarations in the downloaded copy of
# the library, so they stay in flash. Nothing reads them, so how they are read
# does not matter. Idempotent; a library without the file or the arrays is left
# alone, and the build still works, only with less heap.
Import("env")
import re
from pathlib import Path

if env.get("PIOPLATFORM") == "espressif8266":
    header = (Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV")
              / "GFX Library for Arduino" / "src" / "YCbCr2RGB.h")
    if header.exists():
        text = header.read_text(encoding="utf-8")
        patched = re.sub(r"^static const (u?int16_t) (\w+)\[\] =", r"static const \1 \2[] PROGMEM =", text, flags=re.M)
        if patched != text:
            header.write_text(patched, encoding="utf-8")
            print("gfx_tables_to_flash: JPEG colour tables moved to flash")
