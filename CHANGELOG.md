# Changelog

The firmware is `geekmagic-<version>`; every release adds 0.01.

## 1.47 — 2026-11-14

- The crash record says more: what the firmware was doing (network, a web
  request, the apps, the weather or a watch face's data request), the free
  heap then and its low since start, the Wi-Fi sleep type, and the first code
  addresses on the stack, so a fault inside the Wi-Fi driver (the software
  watchdog in `pm_send_nullfunc`, seen twice) can be traced to its cause.
  A record from an older version is not read.

## 1.46 — 2026-11-14

- The same as 1.44. (1.45, never released, tried an Exchange Rates app; it
  was taken out again.)

## 1.44 — 2026-11-13

- Pomodoro: the long break is called *Отдых* on the Russian clock screen;
  *Длинный перерыв* did not fit inside the ring.
- Timer: a long name is cut with "..." to fit inside the ring.

## 1.43 — 2026-11-13

- **Display → Notifications → Show for** now sets how long every banner stays,
  the Timer's and Pomodoro's too (the Timer's was fixed at 30 seconds, the
  Pomodoro's at 20). A script's own `ttl` still wins for its message.

## 1.42 — 2026-11-13

- **Pomodoro**, a new app: focus sessions, short breaks and a long break after
  every few sessions, on a ring in the colour of the phase with a dot per
  session. A banner at the end of each phase; the next one can start by
  itself. Durations in its page; `/api/pomodoro` for scripts and Shortcuts.

## 1.41 — 2026-11-13

- **Software Update** checks for a new release when the web page opens and
  installs it with one tap (*Download and Install*). The page, not the device,
  fetches the image from this repository.
- Releases are built, tested and published by GitHub Actions from a version
  tag.
- A changed home-screen icon reaches the phone: the icon and manifest links
  carry a content hash, so a phone no longer adds the app with an old cached
  icon.

## 1.40 — 2026-11-13

- Modular and Utility are built into the firmware: always on the device,
  restored at start if missing, updated with the firmware, and not removable.
  A new device starts on Modular.
- **Erase All Content and Settings** (was *Erase All Settings*) also removes
  the reminders, the watch faces you added and the crash record; the built-in
  faces stay.
- Web page: a link to this repository at the bottom instead of the version; no
  address line under About (it is in the list); as a home-screen app it no
  longer zooms or scrolls past its edges.

## 1.39 — 2026-11-13

- Web page on a phone: the Name row keeps its label, field and button apart
  while a new name is typed; the tab bar fits a 375 px screen.

## 1.38 — 2026-11-13

- Timer: minutes and seconds centred in their fields, without the step arrows;
  the label field is aligned left.

## 1.37 — 2026-11-13

First release.

- Apps: watch faces (Modular and Utility included), Calendar, Reminders,
  Weather (Open-Meteo, no key), Timer; one at a time or in rotation, with a
  fade between them.
- Over every app: notification banners (`/api/notify`), the running timer, the
  Nightstand clock at night.
- Watch faces read the Weather app's reading (`device.weather*`).
- One language for the device and the web page: English or Russian.
- Web page styled after iPhone Settings, saves as you change; installable as a
  home-screen web app; optional password.
- Brightness by hand, by the light sensor, on a schedule or from sunset to
  sunrise; panel colour calibration.
- Wi‑Fi: up to four networks, setup hotspot with a QR code, a watchdog that
  rejoins a silent network.
- About 25 KB of free heap at rest.
