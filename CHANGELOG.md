# Changelog

The firmware is `geekmagic-<version>`; every release adds 0.01.

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
