# Development

## Requirements

- [PlatformIO](https://platformio.org) (`pip install platformio`)
- A C++11 compiler, Node.js, Python 3.9+ with Pillow (`pip install -r tools/requirements-theme.txt`)

## Build and flash

```sh
pio run                          # .pio/build/geekmagic/firmware.bin
pio run -t upload                # over USB
curl -F firmware=@.pio/build/geekmagic/firmware.bin http://geekmagic.local/update
```

Or upload the file in the web page, **General → Software Update**.

After editing `src/webui.html`, regenerate the served copy:

```sh
python3 tools/gzip_webui.py      # writes src/webui.h
```

## Tests

```sh
PYTHON=python3 sh tests/run_all.sh
```

runs every host test: the device screens, the apps' views, settings, the status
reply, the watch face engine and tools, the web page's scripts and its
translation, and a pixel-for-pixel check of the page's face preview against the
firmware's renderer. The 32-bit memory tests run on Linux only. `tests/fuzz`
holds libFuzzer targets for the parsers of untrusted input (see its README).

Tests compile the firmware's own sources without Arduino, so code in the
`*View.cpp`, `ThemeEngine`, `Settings` and `StatusJson` files must stay
host-buildable.

## Versions

The firmware is called `geekmagic-<version>` (`FW_NAME`, `FW_VERSION` in
`src/config.h`). The version goes up by 0.01 with every release: 1.36, 1.37, …
Release images are attached to GitHub releases as `geekmagic-<version>.bin`, and
every release gets an entry in [CHANGELOG.md](../CHANGELOG.md).

## Conventions

- **Screen safe area.** The case hides the panel's edges: keep anything that
  must be seen at least 20 px inside (`kSafe`); the tests check it.
- **Words on screen** go through `SysText` or a view's `PROGMEM` table in both
  languages, never as literals in code.
- **The web page** is styled after iPhone Settings: grouped lists, short
  labels, no explanatory paragraphs. Every new string goes through `lz()` and
  gets a Russian entry in `RU`.
- **Settings** never take a blank password as "clear it": blank keeps the stored
  one, and clearing is its own explicit action.
- **Memory:** check RAM in the `pio run` output and `heap`/`minHeap` in
  `/api/status` after a change; see [architecture.md](architecture.md#memory).

## Watch faces

```sh
python3 tools/faces.py build faces/modular faces/modular.stheme
python3 tools/faces.py preview faces/modular /tmp/modular.html --data device.weatherTemp=5
curl -F theme=@faces/modular.stheme http://geekmagic.local/api/themes/install
```

The format is documented in [watch-faces.md](watch-faces.md). Modular and
Utility are also built into the firmware: after rebuilding either package, run
`python3 tools/gen_default_faces.py` to update `src/DefaultFaces.h`
(`tests/run_all.sh` checks they match).
