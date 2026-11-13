# Apps and HTTP API

The screen shows one app at a time, or rotates through the apps you tick
(**Apps → Rotate**). Notifications, a running timer and the Nightstand are drawn
over whatever app is showing.

Every endpoint below needs the web password when one is set (HTTP digest).
`geekmagic.local` is the default name; use the device's own name or IP.

## Watch face

Shows the selected watch face. Modular and Utility are built into the firmware
(`src/DefaultFaces.h`): they are always on the device, written again at start
when missing or older than the image, cannot be removed, and a new device shows
Modular. See
[watch-faces.md](watch-faces.md) for the package format and `/api/themes`.

## Calendar

This month as a grid, today on a red circle; weeks start on Monday (Sunday in
English). Names follow the device language. Until the clock has been set over
the internet, the screen says it is waiting.

## Weather

The current weather at the device location (**General → Location**) from
[Open-Meteo](https://open-meteo.com): temperature, condition, today's high and
low, and the next hours. Fetched every 15 minutes while the app, or a watch
face that shows the weather, is on the screen. When `api.open-meteo.com` cannot
be reached, the device uses `historical-forecast-api.open-meteo.com`.

| Request | Does |
| --- | --- |
| `GET /api/weather` | `{"valid":true,"temp":2.1,"feels":-2.9,"humidity":83,"wind":19.4,"code":53,"cond":5,"day":true,"high":2,"low":1,"hours":[…],"age":120,"host":"…"}` (`wind` in km/h, `age` in seconds) |
| `POST /api/weather` | Fetches again the next time the app or such a face is on the screen |

Watch faces read the same reading as `device.weather*` values.

## Reminders

A short to-do list that stays on the screen until it is cleared, and survives a
restart. Up to 12 items of up to 63 bytes; a full list makes room by dropping
its oldest completed item.

`GET /api/reminders` → `{"open":N,"max":12,"items":[{"text":"…","done":false}]}`.

`POST /api/reminders` with any of these keys (applied in this order, all or
nothing), answers the new list:

| Key | Value | Does |
| --- | --- | --- |
| `items` | array of strings or `{"text","done"}` | Replaces the list |
| `add` | string or array of strings | Adds to the end |
| `done` / `undone` | index | Marks an item done or open |
| `remove` | index | Deletes an item |
| `clearDone` | `true` | Deletes the completed items |
| `clear` | `true` | Deletes everything |

```sh
curl -X POST http://geekmagic.local/api/reminders -d '{"add":"Buy milk"}'
```

## Timer

One countdown with a ring. Starting it puts it over the current app; at zero a
banner says so and the screen goes back.

`POST /api/timer`: `start` (1–86399 s) with optional `label` and `show`
(default `true`), or `pause`, `resume`, `add` (seconds, may be negative),
`cancel`, `show`, `hide`. `GET /api/timer` and every `POST` answer
`{"state":"idle|running|paused","left":…,"total":…,"label":"…","show":true}`.

```sh
curl -X POST http://geekmagic.local/api/timer -d '{"start":300,"label":"Tea"}'
```

## Nightstand

With **Display → Night** set to a schedule or to sunset-to-sunrise, the
Nightstand switch shows a large dim amber clock over every app at night.

## Notifications

`POST /api/notify` shows a banner over the current app, then gives the screen
back. Every field is optional; the smallest request is `{"label":"…"}`.

| Field | Meaning |
| --- | --- |
| `type` | `info` (default), `warning`, `alert`, `done`, `waiting`: sets colour, hold time and priority |
| `label` | The message, wrapped over up to five lines |
| `title` | A line above the label (up to 20 characters) |
| `color` | `#rrggbb` or `white black red green blue yellow cyan magenta orange gray` |
| `ttl` | Seconds on screen, 2–120; default from **Display → Notifications** |
| `priority` | 0–3; a higher one jumps the queue and takes the screen |

Banners queue rather than overwrite. Answers `{"ok":true,"queued":N}`; an
unknown type or colour answers 400, a full queue with nothing less important to
drop answers 429.

```sh
curl -X POST http://geekmagic.local/api/notify -d '{"type":"alert","label":"Disk 98% full"}'
```

## Brightness

By hand, by the light sensor (**Auto-brightness**), on a night schedule, or
from sunset to sunrise: sunrise and sunset are worked out on the device from
its location, and the screen fades between its normal and lowest brightness
over the set number of minutes.

## Settings and status

| Request | Does |
| --- | --- |
| `GET /api/status` | Version, network, memory, clock, last crash (`lastCrash`) |
| `GET /api/config` | Settings, without passwords |
| `POST /api/config` | Changes the settings in the JSON body (only the keys present); a Wi‑Fi change restarts the device |
| `GET /api/export` / `POST /api/import` | The full settings file, passwords included / restore it and restart |
| `GET /api/scan` | Wi‑Fi networks in range |
| `POST /api/reboot`, `POST /api/factory` | Restart / erase the settings, reminders and watch faces and restart as new |
| `POST /update` | A firmware image as `multipart/form-data` field `firmware` |
