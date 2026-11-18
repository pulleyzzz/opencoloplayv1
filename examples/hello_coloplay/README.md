# hello_coloplay

Starter application for the coloplay v1 hardware. Initializes every BSP
driver and wires them to a small LVGL UI on the round GC9A01 display:

- **Knob (EC11)** — adjusts the volume shown by the arc
- **KEY1** — cycles the SK6812 LED ring mode (off / solid / rainbow)
- **KEY2 / KEY4** — volume −10 / +10
- **KEY3** — records 2 s from the ES8311 microphone and plays it back
- **Knob press (EKEY)** — plays a beep through the speaker (also played at boot)
- **P (PWR)** — toggles the display backlight

## Build & flash

```bash
source <IDF path>/export.sh          # e.g. ~/.espressif/master/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

If the USB port is occupied by the firmware (esptool cannot reset the
chip): power off, **hold the knob press + rear power button** while
powering on to enter the bootloader, then
`idf.py -p PORT --before no_reset flash`. See the
[developer guide](../../docs/developer-guide.md#烧录方式) for details.

Requires ESP-IDF 6.1 or newer.
