# Recovery

- **Erase All Content and Settings** (General → Transfer or Reset) removes the
  settings, the reminders, the watch faces you added and the crash record, and
  restarts like a new device: the setup hotspot `GeekMagic-Setup`, with Modular
  on the screen. The built-in faces and the firmware stay.
- **Setup hotspot.** When no saved network can be joined, the device opens its
  hotspot and shows a QR code to join it; the page is then at
  `http://192.168.4.1`.
- **A crash** restarts the device in safe mode: the screen shows the crash
  address and the web server stays up, so a fixed image can be uploaded. The
  record (`lastCrash` in `/api/status`) stays in General → About until cleared.
  Decode an address with the ELF of the same build:
  `xtensa-lx106-elf-addr2line -pfiaC -e .pio/build/geekmagic/firmware.elf 0x…`.
- **A forgotten password** can only be removed by flashing over USB/UART.
- **Back to stock.** Keep a backup of the stock flash from before the first
  flash (`esptool.py read_flash 0 0x400000 stock-backup.bin`); write it back
  over USB/UART to return the device to its factory firmware.

Pin maps and hardware notes come from the ESPHome and Tasmota communities, for
example [ViToni/esphome-geekmagic-smalltv](https://github.com/ViToni/esphome-geekmagic-smalltv).
