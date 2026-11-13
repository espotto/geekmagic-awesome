# Watch faces

A watch face is a package (`.stheme`) of data and images; it never contains
code. The firmware validates a package when it is installed, keeps up to 16 of
them in flash, and draws the selected one. The two standard faces, Modular and
Utility, are in [`faces/`](../faces).

## Install and select

In the web page, **Apps → Watch face → ⓘ** lists the installed faces with a
preview, adds a `.stheme` file (**+**), puts a face on the screen or removes
it. Over HTTP, see [HTTP API](#http-api) below. A face that cannot be read
stays listed with its error so it can be removed. Settings exports name the
selected face but do not contain the packages: keep the `.stheme` files.

Notifications, the running timer and the Nightstand cover the face and give it
back with the current time.

Installing a package records its name, author, version and size in one index
file, `/themes/.index`, so the list never has to parse every package again.

## Create a source theme

```text
pixel-room/
├── theme.json
├── images/background.png
├── animations/cat/000.png
├── animations/cat/001.png
```

The desktop tools require Python 3.9+, Pillow and a C++11 compiler (`c++`, `g++`,
or `clang++`; set `CXX` if needed). Prepare the dependencies once:

```bash
python3 -m venv .venv && . .venv/bin/activate
pip install -r tools/requirements-theme.txt
pio pkg install
```

PlatformIO provides the ArduinoJson headers used by the firmware.
An existing firmware build already has them. No device or firmware recompilation
is needed: the native desktop adapter compiles automatically on first use and is
cached under `.pio/theme-host/`. Alternatively, point `GEEKMAGIC_ARDUINOJSON_INCLUDE`
at a library source directory containing `ArduinoJson.h`.

Validate, build and preview a theme:

```bash
python3 tools/faces.py validate tests/theme/fixtures/pixel-room
python3 tools/faces.py build tests/theme/fixtures/pixel-room /tmp/pixel-room.stheme
python3 tools/faces.py validate /tmp/pixel-room.stheme
python3 tools/faces.py preview /tmp/pixel-room.stheme /tmp/pixel-room.html \
  --time 2026-09-18T10:24:55 --seconds 10 --fps 15
```

`validate` accepts a source directory or an installed-format `.stheme`. Both paths
use the **firmware's actual C++ validation**, including every compiled frame. Source
validation also checks asset decoding and safe relative paths. Errors identify the
field or asset, for example `layers[0].color: expected a color in #RRGGBB form`.
`build` validates before writing its output. The original
`python3 tools/theme_pack.py SOURCE OUTPUT.stheme` command remains supported and
now performs the same complete validation.

Open the generated HTML in a browser. It contains the images and controls needed
for offline playback, pause, seeking and replay. A ready-made example lives at
`tests/theme/fixtures/pixel-room-preview.html`; simply open it, without installing tools.
Preview generation accepts source directories too. It uses the same C++ compositor,
RGB565 blending and system font as the device, and advances time across day/month/year
boundaries. `--time` is the desired local wall time without a UTC offset, defaulting
to the computer's current wall time. Duration is 1–60 seconds, sampled at 1–15 FPS.
The recorded clip stops at its end; Replay restarts it. A lower preview sampling
rate can skip intermediate animation frames. Panel correction, hardware latency
and notifications are not simulated. The preview's playback controls are desktop
HTML, never included in `.stheme` packages.

A theme that declares `data` fields has nothing to fetch on a desktop, so preview
generation accepts repeatable `--data source.field=value` arguments and holds
those values for every sampled frame instead:

```bash
python3 tools/faces.py preview tests/theme/fixtures/live-status \
  tests/theme/fixtures/live-status-preview.html --seconds 8 --fps 15 \
  --time 2026-09-22T12:00:00 \
  --data status.label='SmallTV dynamic dashboard headline' \
  --data status.level=82 --data status.state=Warning
```

Each key must reference a field declared in the theme's `data` block, exactly
once; an unknown key, a missing `=`, an empty key, a duplicate key, or more than
32 values fails the command before it writes any output. Injected values only
affect the preview frames; they cannot change package validation or which
assets are read. `tests/theme/fixtures/live-status` is the reference fixture for this
workflow — see [Example: a dynamic dashboard](#example-a-dynamic-dashboard).

This utility converts image pixels on your computer. The device reads RGB565
assets directly; it does not decode PNG/GIF/TTF. The package stores the same
manifest values, including logical source paths. A future compiler can replace
the asset provider without changing those paths. GIF extraction, custom fonts,
and advanced compiler features are not implemented in V1. Images are stored
compressed (STI2, see [Device package format](#device-package-format)) only in
a theme with `"spec": 2`.

## Manifest

```json
{
  "spec": 1,
  "theme": {"id": "minimal-clock", "name": "Minimal Clock", "author": "You", "version": "1.0.0"},
  "display": {"width": 240, "height": 240, "background": "#000000"},
  "layers": [
    {"id": "clock", "type": "text", "x": 120, "y": 100, "anchor": "center", "value": "{HH}:{MM}", "size": 48, "color": "#ffffff"},
    {"id": "date", "type": "text", "x": 120, "y": 150, "anchor": "center", "value": "{WD} {DD} {MON}", "size": 16, "color": "#aaaaaa"}
  ]
}
```

All fields shown at the root, in `theme`, and in `display` are required.
`spec` is 1 or 2. Spec 2 lets the packer store images as STI2, which is much
smaller for images with transparent areas or few colours. Firmware up to
3.1.1 refuses a spec 2 theme; when it has STI2 images, the message there is
"Invalid or truncated .stheme package", because that firmware checks the
images before it reads the manifest. A spec 1 theme packs exactly as before. Spec 2
also lets a binding read a [built-in value](#built-in-sources). The
canvas is always 240×240 with the origin at the top left. Coordinates and sizes
are integer pixels. Layers draw in array order, back to front; IDs must be unique.
Unknown fields, unsupported types, invalid values, and unknown text variables are
rejected. Nothing evaluates expressions or runs scripts.

## Small JSON data sources

A theme may declare up to four small JSON sources. The firmware fetches each URL
periodically and exposes only the declared fields to text layers. A response is
limited to 2 KiB, and a source can declare at most eight fields. The minimum
refresh interval is 10 seconds.

One request is in flight at a time, and every request is bounded by a
three-second deadline that header and body reads also check. The fetch is one
blocking call in the main loop, and the display waits for it within the same
deadline. The firmware has no TLS: an `https://` source fails, so use plain
HTTP. Switching themes cancels the old request
and discards its result.
A field may add `"decimals"` (0 to 3) to round a number to that many places,
so `{"id": "temp", "path": "current.temperature_2m", "decimals": 0}` shows `3`
for `2.6` and `0` (never `-0`) for `-0.4`; without it the number is shown as
sent. Strings and `true`/`false` are not rounded.
Failed requests keep the last displayed values until the next scheduled attempt.
When a value changes, only text whose rendered content changed is repainted,
including the area needed to erase its previous contents.

```json
{
  "data": [
    {
      "id": "weather",
      "url": "https://example.local/weather.json",
      "interval": 300,
      "insecureTls": true,
      "fields": [
        {"id": "temp", "path": "main.temp"},
        {"id": "city", "path": "name"}
      ]
    }
  ],
  "layers": [
    {"id": "reading", "type": "text", "x": 120, "y": 90,
     "anchor": "center", "value": "{weather.city} {weather.temp}C",
     "size": 16, "color": "#ffffff"}
  ]
}
```

The `path` is a dotted path through JSON objects. Values are rendered as short
strings; missing values show `--`. URLs must use `http://` or `https://`. Because
this path cannot validate server certificates, an HTTPS source must explicitly
set `"insecureTls": true`; omitting it rejects the package instead of silently
accepting unauthenticated TLS. Data fetching is owned by theme mode and does not
affect notifications or other display modes.

Installing a theme installs its data sources too: the device will periodically
poll every URL the manifest declares, including addresses on your LAN. Only
install themes from sources you trust — a manifest's `data` block is not
sandboxed beyond the request limits above (2 KiB response, one in-flight
request, three-second deadline).

Fetched values are scalars used as-is: to interpolate into text (above), or to
drive a layer's position, size, or color through the bindings below. There are
no history buffers, line charts, arrays, aggregation, arithmetic expressions, or
conditions in V1 — a fetched number maps to a property through exactly one
declared, bounded rule.

Every layer needs `id`, `type`, `x`, and `y`. The device clips geometry outside the
canvas. Colors are exactly `#RRGGBB`, case insensitive; asset alpha is supported.

### Text

Required fields: `value`, `size`, `color`. Optional `anchor` defaults to `top-left`.
Anchors position the complete text cell rectangle:

| Vertical position | Left | Center | Right |
| --- | --- | --- | --- |
| Top | `top-left` | `top-center` | `top-right` |
| Center | `center-left` | `center` | `center-right` |
| Bottom | `bottom-left` | `bottom-center` | `bottom-right` |

Text is drawn in the device's one system font, SF Compact, anti-aliased, in one of
six fixed styles (see [Text styles](#text-styles)). A layer names its style with
`font`; a layer that gives only `size` (8–96, older themes) gets the style nearest
that size. Characters are printable ASCII plus Cyrillic (`А`–`я`, `Ё`, `ё`) and the
degree sign `°`, written as ordinary UTF-8 in `theme.json`; any other character in
a fetched value is drawn as `?`, and a text template that contains one is refused.
The layer's height is the style's line height, its width the sum of the glyph
advances. No wrapping or multiline text. Text is transparent over lower layers.
Month and weekday names are English in a `"spec": 1` theme; the device's configured
timezone applies. In a `"spec": 2` theme they follow the device language
(see [Localized text](#localized-text)).

| Token | Value |
| --- | --- |
| `{HH}` | 00–23 hour |
| `{hh}` | 01–12 hour |
| `{MM}` | 00–59 minute |
| `{SS}` | 00–59 second |
| `{DD}` | 01–31 day |
| `{MON}` / `{MONTH}` | Abbreviated / full month |
| `{WD}` / `{WEEKDAY}` | Abbreviated / full weekday |
| `{YYYY}` | Year |

A theme with `"spec": 2` also accepts these tokens, so one text can use the
union of the tokens below:

| Token | Value |
| --- | --- |
| `{H}` | 0–23 hour, no leading zero |
| `{MO}` | 01–12 month number |
| `{AMPM}` | `AM` for 00:00–11:59, `PM` for 12:00–23:59 |
| `{device.focusToday}`, `{device.focusGoal}`, `{device.streak}`, `{device.humidity}`, `{device.temperature}` | The firmware's value, as text, exactly as it supplied it |
| `{device.weatherTemp}`, `{device.weatherFeel}`, `{device.weatherHigh}`, `{device.weatherLow}`, `{device.weatherHum}`, `{device.weatherWind}`, `{device.weatherCode}`, `{device.weatherText}`, `{device.weatherDay}` | The Weather app's reading at the device location (see below) |

`{hh}` is the 12-hour clock with a leading zero, so `{hh}:{MM} {AMPM}` reads
`03:04 PM`; `{H}:{MM}` reads `15:04`. Like the other clock tokens, the three
clock tokens show `--` until the clock has been set. A `{device.*}` token shows
`--` when the firmware does not supply that value (until the first weather
reading, or always for the five sensor and focus names on this firmware; see
[Built-in sources](#built-in-sources)), so a layout
that must also run here should leave room for `--`. The names are the same as
for bindings; `time.*` is not a text token, because `{HH}`, `{MM}` and the other
clock tokens already cover it.

In a `"spec": 1` theme none of these is accepted: `{H}`, `{MO}`, `{AMPM}` and
`{device.*}` are unknown variables, so the package is refused when it is checked
with `layers[n].value: use printable ASCII or Cyrillic and declared clock/data variables only`,
the same as any other unknown `{...}`. (A spec 1 theme that declares its own
`data` source called `device` keeps using `{device.<field>}` as a fetched
field, as before.) `tools/faces.py preview --data device.temperature=21`
supplies a device value for a spec 2 theme, to see what the layer shows with and
without it.

#### Localized text

Localized text needs firmware v3.4.0 or later. The device has one language setting
(General → Language in the web page; `clock.lang` in the settings file) that the
clock's own screens and the web page both follow. It takes `en` or `ru` and
defaults to `en`. (A stored `pt`, `es`, `fr`, `de` or `it` from older settings reads as `en`.) In a `"spec": 2` theme:

- `{MON}`, `{MONTH}`, `{WD}` and `{WEEKDAY}` come out in that language. Russian
  uses Cyrillic, with `{WD}` as `пн`…`вс` and the months in lower case and, for
  `{MONTH}`, in the genitive (`12 ноября`, `{MON}` `ноя`). A `"spec": 1` theme
  keeps English names, whatever the device language.
- A text layer's `value` may be an object with one string per language instead
  of a single string:

```json
{"id": "greeting", "type": "text", "x": 120, "y": 60, "anchor": "center", "size": 16, "color": "#ffffff",
 "value": {"en": "Today is {WEEKDAY}", "ru": "Сегодня {WEEKDAY}"}}
```

`en` is required and is shown for any language the object does not list, so a
theme never shows an empty layer. The other key is `ru`, in lower case. The former codes `pt`, `es`, `fr`,
`de` and `it` are skipped, so an older theme still installs; any other key is an error. Every string follows the rules of a plain `value`
(1–128 bytes of printable ASCII or Cyrillic, only declared variables), is checked when the package
is installed, and may use any variable. The non-English texts of a whole theme share 1,024 bytes of RAM: each counts
28 bytes and, past 15 characters, its text too, so that is about 36 short ones or
6 of 128 characters (the English ones are not counted). In a `"spec": 1` theme an object is refused with the layer named, so
the package never installs half-working. Changing the device language redraws
the text at once.

`faces.py preview` takes `--lang CODE` to preview a language (default `en`).
Text on a layer whose size or position depends on the language is up to the author:
leave room for the longest string.

#### Scrolling text

A text layer whose rendered content is wider than a declared viewport can
scroll horizontally, instead of being clipped:

```json
{"id": "headline", "type": "text", "x": 20, "y": 40, "value": "{status.label}",
 "size": 12, "color": "#ffffff",
 "scroll": {"width": 200, "mode": "loop", "speed": 30, "pause": 1200, "gap": 24}}
```

| `scroll` field | Type | Range | Required | Meaning |
| --- | --- | --- | --- | --- |
| `width` | integer | 1–240 px | yes | Width of the clipping viewport |
| `mode` | string | `"mode": "loop"`, `"mode": "bounce"` | yes | Motion behavior |
| `speed` | integer | 1–240 px/s | yes | Horizontal speed |
| `pause` | integer | 0–10000 ms | no, default 1000 | Hold time at the initial position, and for bounce, at both ends |
| `gap` | integer | 0–240 px | loop only, default 24 | Blank space between repeated copies |

`gap` is rejected in bounce mode; it has no effect there. `x`, `y`, and `anchor`
position the viewport, not the complete unscrolled string — `x: 120` with
`anchor: center` centers the 200px viewport at x=120, not the full text. The
viewport height is `size`; its width is `scroll.width`. It clips to the 240×240
canvas like any other layer, and text is additionally clipped to the viewport
even when a wider dirty region from another layer overlaps it.

If the rendered text fits within the viewport, it stays fixed at its initial
position and never schedules a repaint. Otherwise scrolling starts after
`pause` milliseconds. In `loop` mode, the text moves continuously in one
direction; once it has scrolled past its own width plus `gap`, it wraps back to
the start and pauses again — the gap is where a second, repeated copy would
begin. In `bounce` mode, the text moves to its far edge, pauses, reverses, moves
back to the start, and pauses again, alternating direction on every cycle.
Timing uses elapsed milliseconds and a subpixel accumulator, the same technique
frame animation already uses; a late update jumps directly to the due integer
pixel offset rather than replaying intermediate frames, and 32-bit millisecond
wraparound is handled correctly. A display write is scheduled only when the
visible integer pixel offset actually changes.

Changing the expanded text, or a binding that affects `size`, `scroll.width`, or
`scroll.speed`, resets scroll position, phase, and direction to their initial
state and restarts the pause. Any other change — an unrelated data value, a
binding affecting `x` or color, or calling the engine's notification-recovery
`invalidate()` — does not reset scroll progress; switching themes does.

#### Text styles

The device has one font, SF Compact, built into the firmware in six styles; every
screen uses it (themes, notifications, the firmware's own screens). A theme picks a
style, not a font or a size, the way an app picks a text style on a phone:

| Style | Font | Size | Line height | Characters | `HH:MM` width |
| --- | --- | --- | --- | --- | --- |
| `caption` | SF Compact Regular | 17 px | 21 px | all | 45 px |
| `body` | SF Compact Medium | 22 px | 27 px | all | 59 px |
| `headline` | SF Compact Medium | 28 px | 34 px | all | 80 px |
| `display-s` | SF Compact Medium | 48 px | 59 px | digits, `% + - . : °`, space | 131 px |
| `display-m` | SF Compact Medium | 70 px | 85 px | the same | 193 px |
| `display-l` | SF Compact Semibold | 96 px | 117 px | the same | 270 px (too wide: use it for 2–3 characters) |

```json
{"id": "time", "type": "text", "x": 120, "y": 94, "anchor": "center",
 "value": "{HH}:{MM}", "font": "display-m", "color": "#f5f5f7"}
```

The display styles carry only digits and a few signs; any other character in them
is drawn from `headline`, on the same baseline. With `font` the layer's `size` is
optional and unused and a `size` binding is refused; `scroll` works with every
style. Without `font`, `size` picks the style: up to 18 `caption`, 24 `body`, 38
`headline`, 58 `display-s`, 82 `display-m`, above that `display-l`. There is no
kerning. The panel's frame covers its outermost pixels, so keep text about 20 px
inside the edges. Packages carry no fonts: the root `fonts` key of earlier local
builds is refused.

### Image

```json
{"id":"background","type":"image","x":0,"y":0,"source":"images/background.png"}
```

Dimensions come from the asset, between 1×1 and 240×240. No runtime scaling.
PNG alpha is preserved and composited against the layers below it.

### Shape

```json
{"id":"box","type":"shape","shape":"rectangle","x":20,"y":20,"width":200,"height":60,"cornerRadius":8,"fill":"#102030","stroke":"#ffffff","strokeWidth":2}
```

```json
{"id":"ring","type":"shape","shape":"circle","x":120,"y":120,"radius":110,"stroke":"#ffffff","strokeWidth":2}
```

```json
{"id":"line","type":"shape","shape":"line","x":20,"y":180,"x2":220,"y2":180,"stroke":"#ffffff","strokeWidth":2}
```

Rectangles use a top-left origin; circles use a center and radius. Rectangle and
circle strokes are inside their outer bounds. Lines use two endpoints, with a
centered stroke and round ends. `strokeWidth` defaults to 1 and supports 1–32.
Rectangles and circles need `fill`, `stroke`, or both; lines need `stroke`.

A rectangle's `cornerRadius` is optional, defaults to 0 (square corners), and
has a static manifest range of 0–120 px. After any bindings resolve, the
effective radius is clamped to `floor(min(width, height) / 2)`, so a rectangle
that shrinks (through a bound `width` or `height`) never draws rounding outside
its own bounds. Fill occupies the rounded outer shape; stroke occupies the ring
between that outer shape and an inner shape whose radius is
`max(0, cornerRadius - strokeWidth)`. A dynamic width or height that resolves to
zero makes the rectangle temporarily invisible, regardless of `cornerRadius`.

#### Arcs, hands and tick marks (spec 2)

Three more shapes exist only in a theme with `"spec": 2`, on firmware v3.4.0
or later. A spec 1 theme that
uses one is refused with `arc, hand and ticks need "spec": 2`, and a spec 1
theme installs exactly as before. All three are drawn in `stroke` alone, with no
`fill`, and take an optional `strokeWidth` of 1–32 (default 1).

```json
{"id":"progress","type":"shape","shape":"arc","x":120,"y":120,"radius":100,"start":0,"sweep":270,"stroke":"#47c2ff","strokeWidth":10}
```

```json
{"id":"minute","type":"shape","shape":"hand","x":120,"y":120,"length":90,"tail":20,"angle":0,"stroke":"#ffffff","strokeWidth":4}
```

```json
{"id":"dial","type":"shape","shape":"ticks","x":120,"y":120,"radius":112,"count":12,"length":10,"stroke":"#8a8a8a","strokeWidth":3}
```

Angles are whole degrees, **0 at 12 o'clock and growing clockwise**, so 90 is
3 o'clock and a negative angle goes the other way (−90 is 9 o'clock).

* **`arc`** draws a ring segment about the center `x`, `y`: the outer edge is
  `radius` and the band is `strokeWidth` wide inside it, like a circle's stroke.
  It starts at `start` (−360 to 360) and runs `sweep` degrees (−360 to 360)
  clockwise, or counter-clockwise when `sweep` is negative. A `sweep` of 360 or
  more is a full ring and a `sweep` of 0 draws nothing, so a progress ring at 0 %
  is simply empty. A `strokeWidth` of at least `radius` fills the wedge. The
  ends are flat and radial. `radius` is 1–240.
* **`hand`** draws a line through the pivot `x`, `y`, from `tail` (0–240,
  default 0) behind it to `length` (1–240) in front, in direction `angle` (−360
  to 360), with a centered stroke and round ends like `line`.
* **`ticks`** draws `count` (1–60) evenly spaced radial marks around the center
  `x`, `y`, the first at 12 o'clock. Each runs `length` (1–240, stopped at the
  center) inward from `radius` (1–240), with round ends.

`sweep` on an arc, `angle` on a hand and `radius` on an arc or ticks can be bound
(see [Dynamic bindings](#dynamic-bindings)), with an `output` of −360 to 360 for
`sweep` and `angle`. A minute hand is `angle` bound to the minute with `input`
`[0, 60]` and `output` `[0, 360]`; a progress ring is `sweep` bound to a
percentage with `input` `[0, 100]` and `output` `[0, 360]`. Nothing else about
these shapes moves: `start`, `length`, `tail` and `count` are fixed.

Everything is drawn with integers, with no framebuffer and no table: a pixel is
placed to within about a pixel of the exact geometry. When a bound angle or
sweep changes, only the shape's old and new bounding boxes are repainted, so a
single hand repaints two small boxes, not the dial behind it.

### Animation

```json
{"id":"cat","type":"animation","x":160,"y":170,"width":64,"height":64,"source":"animations/cat","frames":12,"fps":8,"loop":true}
```

All shown fields are required. Source frames are `000.png`, `001.png`, etc.; their
dimensions must exactly match `width` and `height`. Each animation has its own
elapsed-time schedule, independent of clock text. Late rendering skips to the due
frame instead of queuing work. `loop:false` holds the final frame. Pausing under a
notification catches up when the theme becomes visible. Switching themes restarts
the animation. Only the required pixel rows are read, never a whole animation.
The renderer resolves each visible image/frame once per render pass and reuses
its asset handle across rows and dirty regions.

## Dynamic bindings

`bind` is an optional object on any layer, keyed by a supported property name.
Each key holds exactly one numeric mapping or color-stop binding, driven by a
fetched scalar. A binding's `source` is the same declared
`<data-source-id>.<field-id>` token accepted by text interpolation or, in a
`"spec": 2` theme, a [built-in source](#built-in-sources). Bindings are
recomputed from the parsed theme and the latest fetched values whenever those
values change (and, for a binding on `time.*`, whenever the clock moves); the
parsed manifest itself is never mutated. Only the changed
region is repainted — the union of the layer's old and new visible bounds.

### Numeric mapping

```json
{"bind": {"width": {"source": "status.level", "input": [0, 100], "output": [0, 200], "clamp": true}}}
```

| Field | Type | Required | Rule |
| --- | --- | --- | --- |
| `source` | string | yes | A declared data field or, in spec 2, a built-in source |
| `input` | two finite numbers | yes | Endpoints must differ; ascending or descending is allowed |
| `output` | two integers | yes | Both must fit the target property's safe range below |
| `clamp` | boolean | no, default true | Clamp to the output endpoints when true |

The mapping is linear:

```text
output0 + (source - input0) * (output1 - output0) / (input1 - input0)
```

The result is rounded to the nearest integer, halves away from zero. When
`clamp` is true, the source value is limited to the interval between the two
`input` endpoints before interpolation. When `clamp` is false, extrapolation
beyond that interval is permitted, but the final integer is still clamped to
the property's safe range below — that clamp cannot be disabled. A missing,
partial, or non-finite fetched value leaves the static manifest value in
effect for that property, so a theme always has a valid fallback appearance.

| Property | Safe range |
| --- | --- |
| `x`, `y`, `x2`, `y2` | −240 through 479 |
| `width`, `height`, `radius` | 0–240 |
| `cornerRadius` | 0–120, then limited to half the resolved rectangle size |
| `size` | 8–96 |
| `strokeWidth` | 1–32 |
| `scroll.width` | 1–240 |
| `scroll.speed` | 1–240 |

### Color stops

```json
{"bind": {"fill": {"source": "status.level",
  "stops": [{"at": 0, "value": "#35c46a"}, {"at": 60, "value": "#f0b429"}, {"at": 80, "value": "#e5484d"}]}}}
```

| Field | Type | Required | Rule |
| --- | --- | --- | --- |
| `source` | string | yes | A declared data field or, in spec 2, a built-in source |
| `stops` | array | yes | 1–8 entries |
| `stops[].at` | finite number | yes | Strictly increasing |
| `stops[].value` | `#RRGGBB` | yes | Existing color syntax |

The selected color is the last stop whose `at` is less than or equal to the
fetched value; a value below the first stop's `at` uses the first stop's color.
Every finite fetched number therefore has a color, with no separate default
branch. A missing or non-finite fetched value leaves the static manifest color
in effect.

### Property matrix

Each property may be bound once; a layer may bind at most eight properties. A
color property may be bound only when that property already exists statically
on the layer — binding `stroke` does not implicitly enable a missing stroke,
and `scroll.width`/`scroll.speed` require a static `scroll` object. Image and
animation dimensions are not bindable, because runtime asset scaling is
unsupported and their declared size must match packaged assets.

| Layer | Numeric properties | Color properties |
| --- | --- | --- |
| text | `x`, `y`, `size`, `scroll.width`, `scroll.speed` | `color` |
| rectangle | `x`, `y`, `width`, `height`, `cornerRadius`, `strokeWidth` | `fill`, `stroke` |
| circle | `x`, `y`, `radius`, `strokeWidth` | `fill`, `stroke` |
| line | `x`, `y`, `x2`, `y2`, `strokeWidth` | `stroke` |
| arc (spec 2) | `x`, `y`, `radius`, `sweep`, `strokeWidth` | `stroke` |
| hand (spec 2) | `x`, `y`, `angle`, `strokeWidth` | `stroke` |
| ticks (spec 2) | `x`, `y`, `radius`, `strokeWidth` | `stroke` |
| image | `x`, `y` | none |
| animation | `x`, `y` | none |

An inapplicable property, a duplicate binding target, an unknown binding key,
or a source that is not a declared data field rejects the package, for example
`layers[0].bind.width: binding is not applicable to this layer` or
`layers[0].bind.width.source: expected a declared data field`. Invalid `scroll`
fields are diagnosed the same way, for example `layers[0].scroll.mode: expected
loop or bounce` or `layers[0].scroll.gap: not allowed in bounce mode`.

### Built-in sources

A theme with `"spec": 2` may name a value the firmware already has instead of a
fetched field, and everything above applies to it unchanged: the same numeric
mapping and clamping, the same color stops, the same fallback to the layer's
static value when there is no value. Use them exactly like a data field, for
example a seconds bar that needs no `data` block:

```json
{"id": "sec", "type": "shape", "shape": "rectangle", "x": 20, "y": 200, "width": 1, "height": 6, "fill": "#ff5a47",
 "bind": {"width": {"source": "time.second", "input": [0, 59], "output": [0, 200]}}}
```

| Source | Range | Meaning |
| --- | --- | --- |
| `time.second` | 0–59 | Second of the minute |
| `time.minute` | 0–59 | Minute of the hour |
| `time.hour` | 0–23 | Hour, 24-hour clock |
| `time.hour12` | 0–11 | Hour, 12-hour clock (midnight and noon are 0) |
| `time.secondOfDay` | 0–86399 | Seconds since midnight |
| `time.day` | 1–31 | Day of the month |
| `time.month` | 1–12 | Month, January is 1 |
| `time.weekday` | 0–6 | Day of the week, Sunday is 0 |
| `time.moon` | 0–29 | Age of the moon in whole days: 0 is the new moon, 14–15 the full moon |

`time.moon` needs no network. It counts the time since a reference new moon,
2000-01-06 18:14 UTC, modulo the mean synodic month of 29.530588 days (29 d 12 h
44 min 3 s), and drops the fraction. The result is 0 to 29, and it steps up by
one every 24 hours, then falls from 29 to 0 at the next new moon. Because it
uses the mean month, the real moon is up to about 0.75 days early or 0.4 days
late: checked against the US Naval Observatory's times for 150 new and full
moons from 2020 to 2030, a true new moon reads 28, 29 or 0 and a true full moon
reads 14 or 15. Use `input: [0, 29]` and treat 28 to 1 as new and 13 to 16 as
full if a theme needs a safe margin. The value is computed from the clock in
the configured timezone as if it were UTC, which adds up to the offset
(at most about half a day) to that error. Before the clock is set there is no
value.

`time.*` follows the same clock as `{HH}` and the other date and time text
variables, in the configured timezone. Until the clock has been set (no NTP
yet) there is no value, so the layer keeps its static value. A layer bound to a
`time.*` source repaints only when the bound property actually changes: a
`time.second` bar repaints its own rectangle once a second, a `time.minute`
bar once a minute, and neither makes the rest of the canvas redraw.

`device.*` sources are optional values a firmware supplies from something the
theme engine itself does not know about. The names are fixed:
`device.focusToday`, `device.focusGoal`, `device.streak`, `device.humidity` and
`device.temperature`, and the weather ones: `device.weatherTemp`,
`device.weatherFeel` (feels like), `device.weatherHigh`, `device.weatherLow`
(whole degrees C), `device.weatherHum` (%), `device.weatherWind` (m/s),
`device.weatherCode` (the WMO code), `device.weatherText` (the condition in
words, in the device language) and `device.weatherDay` (1 by day, 0 at night).
A firmware that does not supply one leaves the layer on its static value,
exactly like a fetched field that is missing, so one theme can run on
firmwares with and without it. This firmware has no sensor and no focus timer, so it supplies none of those; it
supplies the weather ones from the Weather app (Open-Meteo at the location set
in General), fetched every 15 minutes while such a face is on the screen,
whether or not the Weather app itself is. The engine takes them from the
firmware through `Engine::setDeviceValues()`; they are strings, numbers except
`device.weatherText`, like fetched fields. `GET /api/themes/data` lists them
under `device`. A `device.*`
value is never read from a theme's `data` block, and a fetched field cannot
shadow one.

In a `"spec": 1` theme these names are refused when the package is checked, with
`layers[0].bind.width.source: time.* and device.* sources need "spec": 2`. Spec 1
keeps its rule that `source` must be a declared data field. In spec 2, `time`
and `device` can no longer be used as the `id` of a `data` source, since
`time.second` would then be ambiguous. A name that starts with `time.` or
`device.` but is not in the tables is rejected with `unknown time.* or device.* name`.

## Example: a dynamic dashboard

`tests/theme/fixtures/live-status` builds `tests/theme/fixtures/live-status.stheme` from a
theme declaring one data source (`status`, with `label`, `level`, and `state`
fields) and every capability above:

* `headline` — a `loop`-scrolling `{status.label}` text viewport;
* `headline-bounce` — a second, `bounce`-scrolling label, so both scroll modes
  are copyable side by side;
* `level-track` and `level-bar` — a rounded track and a bar whose `width` maps
  fetched level 0–100 to 0–200 px, with `fill` selected from green/amber/red
  color stops at the same thresholds;
* `level-caption` — status text whose `color` is bound from the same numeric
  `status.level` field, through color stops rather than a numeric mapping.

Every layer also has valid static fallback geometry and colors, so the theme
produces a correct first frame even before any fetch completes. Rebuild and
preview it with the `--data` workflow documented above.

## Limits and storage

| Item | V1 limit |
| --- | --- |
| Manifest | 16 KiB, nesting depth 8 |
| Layers | 32 |
| IDs | 1–48 ASCII letters, digits, `_`, `-` |
| Theme name / author / version | 96 / 96 / 32 bytes |
| Text template | 1–128 UTF-8 bytes of printable ASCII, Cyrillic or `°` |
| Text | One of the six system text styles; no custom fonts |
| Localized text | Up to 7 languages, 1–128 such bytes each, `en` required; 1,024 bytes of non-English text per theme; spec 2 only |
| Coordinates | −240 through 479, clipped to the canvas |
| Rectangle/image/frame dimensions | 1–240 pixels |
| Circle radius | 1–240 pixels |
| Arc / ticks radius | 1–240 pixels |
| Arc start, sweep; hand angle | −360 to 360 degrees |
| Hand length / tail; tick length | 1–240 / 0–240 / 1–240 pixels |
| Tick count | 1–60 |
| Animation | 1–240 frames, 1–15 FPS |
| Container | 256 entries, at most 3 MiB |
| Installed packages | 16, within available storage |

Packages live under `/themes/<id>.stheme`. Upload uses `/themes/.upload`; every
entry, manifest, and referenced animation frame is checked before a rename makes
the package visible. Failed or interrupted uploads leave existing themes and
selection intact. Boot removes an abandoned staging file. Installation reserves
space for settings and filesystem metadata. Settings saves use a temporary file,
check the complete byte count, then rename over the old config. Reading,
saving and importing settings free the running theme first when the heap is
short, and a save that still runs out of memory fails with "not enough
memory" and keeps the old settings.

Installing, selecting and removing a theme, and listing a package that has no
line in the theme index yet, each check a package by loading it. When the free heap is short of what that needs the running theme is released first and loaded
again afterwards, so only one theme is in RAM at a time. It then redraws once
and its animations restart; its fetched values stay, and no data source is
fetched before its interval is up. The manifest is read straight from the package and parsed one
layer at a time, and no list in the engine needs one large block of memory;
on the ESP8266 that is what makes this fit beside the web server.

The engine holds a bounded entry index and layer state in RAM. Compositing uses
row buffers, not a 115,200-byte framebuffer. Static scenes produce no display
writes after the first draw. Clock text is compared when wall time changes;
animations invalidate only when their visible frame changes. Scrolling text
schedules a write only when its visible integer pixel offset changes, and
bindings are recomputed, and repainted, only when their fetched values actually
change. Dirty areas are recomposited through all intersecting layers, restoring
old glyphs, transparent sprite pixels, and foreground layers correctly, and a
scrolling layer's glyphs stay clipped to its own viewport even under a wider
dirty region from another layer.

10–15 FPS and clock updates under 100 ms remain hardware performance targets.
The automated tests exercise composition and scheduling; they do not measure
LittleFS/SPI latency on an actual SmallTV Pro.

## Device package format

`.stheme` V1 is a small uncompressed binary container, **not a ZIP file**. All
integers are unsigned little endian. No extracted paths become filesystem paths.

| Container field | Encoding |
| --- | --- |
| Magic | Four ASCII bytes `STH1` |
| Entry count | `uint16`, 1–256 |
| Reserved | `uint16`, zero |
| Each entry | `uint16` path byte count, `uint32` payload byte count, path bytes, payload |

Paths are relative ASCII letters/digits plus `/`, `.`, `_`, `-`, at most 120 bytes.
Empty components, `.` and `..`, absolute paths, backslashes, and duplicates are
rejected. There must be one `theme.json`. Other entries must be `.sti` images.
No padding, trailing bytes, executable entries, or external asset references are
accepted. JSON data-source URLs remain allowed through the manifest's `data` block.

The asset resolver appends `.sti` to an image's logical `source`, or resolves
`<animation source>/<zero-padded three-digit frame>.png.sti`. For example,
`images/background.png` maps to `images/background.png.sti` inside the package.

| STI field | Encoding |
| --- | --- |
| Magic | Four ASCII bytes `STI1` |
| Width, height | Two `uint16`, each 1–240 |
| Alpha flag | `uint8`: 0 for opaque, 1 for alpha |
| Reserved | Three zero bytes |
| Pixels | Row-major RGB565 `uint16`, followed by alpha `uint8` if flag=1 |

Alpha 0 is transparent, 255 opaque. Intermediate values blend RGB565 channels
with rounding. Opaque files use two bytes/pixel, transparent files three. The
payload length must match the dimensions exactly.

A theme with `"spec": 2` may also contain STI2 images, which store each row as
runs. The packer writes whichever is smallest per image: STI1, STI2 with
RGB565 colours, or STI2 with a palette of up to 16 colours. A spec 1 package
with an STI2 image is rejected, with the image named.

| STI2 field | Encoding |
| --- | --- |
| Magic | Four ASCII bytes `STI2` |
| Width, height | Two `uint16`, each 1–240 |
| Alpha flag | `uint8`: 0 for opaque, 1 for alpha |
| Palette size `P` | `uint8`: 0 for RGB565 colours, 1–16 for a palette |
| Reserved | Two zero bytes |
| Palette | `P` colours: RGB565 `uint16`, followed by alpha `uint8` if flag=1 |
| Row table | One `uint32` per row: where the row starts, counted from the end of the table |
| Rows | Each row as runs |

Offsets never go backwards; rows with equal offsets share their data. A row
ends at the next larger offset, or at the end of the image. A run starts with
one byte: its kind in the top two bits, its length minus one (1–64 pixels) in
the low six:

| Kind | Run |
| --- | --- |
| `00` | Transparent pixels, no payload; only in an image with alpha |
| `01` | One colour repeated: RGB565 (+ alpha), or with a palette one index byte |
| `10` | That many colours: RGB565 (+ alpha) each, or with a palette two 4-bit indices per byte, high nibble first; the unused low nibble of an odd run is 0 |
| `11` | Reserved; the image is rejected |

The device checks every row when the package is installed or loaded: the runs
of a row must cover its width exactly without reading past the row's end, and
every index must be below `P`. The shared test files in `tests/theme/sti/`
show every form with the pixels it must decode to.

## HTTP API

All routes use the device's existing optional digest authentication, including
upload callbacks. Responses are JSON; failures contain `error`.

| Method and route | Request / result |
| --- | --- |
| `GET /api/themes` | All installed packages with `valid`, `error`, filename-derived `id`, metadata, bytes; selected ID, free bytes, renderer error |
| `POST /api/themes/install` | One multipart file; returns `201` and `{ "id": "..." }` |
| `POST /api/themes/select` | JSON `{ "id": "..." }`; validates, saves, activates, starts NTP |
| `POST /api/themes/delete` | JSON `{ "id": "..." }`; rejects deletion of a valid active theme; permits removal of a corrupt selected package |
| `GET /api/themes/data` | The running theme's fetched values and their freshness: `{"values": {"weather.temp": "2"}, "age": 42, "fails": 0}`; `age` is seconds since the last good fetch (`-1` before one), `fails` the failed fetches since |
| `GET /api/themes/package?id=...` | The installed `.stheme` as it is (for the web UI's previews); `400` for an invalid ID, `404` when missing |
| `GET /api/font` | The system font as one binary blob (`writeSystemFont` in `SystemFont.h`), cached by the browser for a year; ask with the firmware version in the query |

Invalid uploads return 400; duplicate IDs and valid active-theme deletion return 409; missing
themes return 404. The normal settings JSON also exposes `mode: "theme"` and
`themeId`. Prefer the selection endpoint because it validates the installed file
before saving. An unavailable selected package displays an error while the web UI
remains available for recovery.

## Development checks

```bash
sh tests/theme/run.sh
python3 -m unittest discover -s tests/theme -p 'test_*.py'
node tests/theme/test_webui.js
```

`tests/run_all.sh` runs these with every other test. The native suite uses the
same C++ parser, scheduler, package reader and compositor as the firmware.
The test faces are in `tests/theme/fixtures/`.
