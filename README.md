# Clawd Desk Buddy

A small, funny desk companion: a 3D printed figure with a display in its face
that shows animated pixel eyes.

> Work in progress. The full documentation follows once the firmware is complete.

## Required hardware

**Waveshare ESP32-C6-Touch-LCD-1.47** (the version with capacitive touch).

The non-touch board **ESP32-C6-LCD-1.47 is not supported**. It looks similar,
but uses different pins and has no touch controller.

## Building with PlatformIO

```bash
pio run -t upload
```

The project uses the [pioarduino](https://github.com/pioarduino/platform-espressif32)
platform (Arduino core 3.x) and needs PlatformIO Core 6.2.0 or newer.

## Credits

- Display register values and touch controller facts for this board come from
  the official [Waveshare documentation and example package](https://docs.waveshare.com/ESP32-C6-Touch-LCD-1.47).
- Graphics: [LovyanGFX](https://github.com/lovyan03/LovyanGFX)

## Disclaimer

This is a fan project. It is not affiliated with, endorsed by or sponsored by
Anthropic. "Claude" and "Clawd" are trademarks of Anthropic.

## License

[MIT](LICENSE)
