# Changelog

The firmware is `geekmagic-<version>`; every release adds 0.01.

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
