# Clawd Desk Buddy

A small, funny desk companion: a 3D printed figure with a display in its face
that shows animated pixel eyes. It blinks, looks around, reacts when you tap
it and goes to sleep at night. That is all it does, and that is the point.

<!-- TODO: add photos -->
![Photo placeholder](docs/images/photo-placeholder.svg)

**3D model:** <!-- TODO: add the MakerWorld link --> coming soon on MakerWorld

## Features

- 24 eye expressions in a chunky pixel-art style, each with its own idle
  animation: Normal, Happy, In love, Excited, Curious, Surprised, Suspicious,
  Sleepy, Sad, Grumpy, Bored, Shy, Wink, Thinking, Confused, Dizzy, Content,
  Vibing, Cool, K.O., Sheepish, Money, Touched and Starstruck
- Smooth transitions between expressions
- Random mode with an adjustable interval
- Touch: tap for a happy reaction, tap again to switch to the next expression
- Web UI for phone and computer at `http://clawd.local`
- Background and eye color freely selectable, with live preview
- Sleep schedule with a sleepy and a wake up animation, tap to wake
- WiFi setup with a captive portal or directly in the web flasher
- Firmware updates through the web UI
- Optional password protection
- No cloud, no account, no tracking. Everything runs on the device.

## Required hardware

**Waveshare ESP32-C6-Touch-LCD-1.47**, the version **with capacitive touch**.

> [!WARNING]
> The non-touch board **ESP32-C6-LCD-1.47 is not supported**. It looks
> similar, but it uses different pins and a different display controller. The
> display stays dark with this firmware.

Power comes from the USB-C port. No battery is needed.

## Flashing

### Web flasher (recommended)

1. Open the web flasher in Chrome, Edge or Firefox 151 or newer on a computer
   (Safari, iPhone and iPad are not supported):
   **<https://pxwaldy.com/clawd-desk-buddy/>**
2. Connect the board with a USB-C data cable.
3. Press **Install Clawd Desk Buddy** and choose the serial port of the board.
4. When flashing is done, enter the name and password of your WiFi network
   when the flasher asks for it.

The BOOT button is not needed, so this also works when the board is already
inside the figure.

### PlatformIO

For developers. Requires [PlatformIO Core](https://platformio.org/install/cli)
6.2.0 or newer.

```bash
git clone https://github.com/PXWaldy/clawd-desk-buddy.git
```

```bash
cd clawd-desk-buddy
```

```bash
pio run -t upload
```

The project uses the [pioarduino](https://github.com/pioarduino/platform-espressif32)
platform (Arduino core 3.x), which PlatformIO downloads on the first build.

### esptool

Download the `-factory.bin` file from the
[releases page](https://github.com/PXWaldy/clawd-desk-buddy/releases) and
write it to address 0:

```bash
esptool --chip esp32c6 write-flash 0x0 clawd-desk-buddy-<version>-factory.bin
```

## First setup

If you did not set up WiFi in the web flasher:

1. After the first start the buddy opens the WiFi network **`Clawd-Setup`**
   (no password). The display shows a hint.
2. Join this network with your phone. The setup page opens by itself. If it
   does not, open `http://192.168.4.1`.
3. Choose your home network, enter the password and press **Connect**.
4. The display shows the IP address for a few seconds. Switch your phone back
   to your home network and open **`http://clawd.local`**.

The setup network is only open while the setup is running. It closes 20
seconds after a successful connection and after 10 minutes if nobody joins.
To open it again later, hold a finger on the display for 5 seconds.

If the home network is not reachable, the buddy retries a few times and then
opens the setup network again.

## Touch

| Gesture | What happens |
|---|---|
| Tap | Clawd looks happy for about 2 seconds, then returns to the previous expression |
| Tap again while it is happy | Switches to the next enabled expression |
| Tap while asleep | Wakes the display for a while (30 seconds by default) |
| Hold for 5 seconds, then release | Opens the WiFi setup network |
| Keep holding for 15 seconds | Factory reset: erases all settings including WiFi |

While you hold, the display shows what releasing will do and counts down to
the factory reset. Releasing earlier cancels it.

## Web UI

Open `http://clawd.local` (or the IP address) in a browser in the same WiFi.
Changes apply immediately and are saved on the device.

| Section | What you can do |
|---|---|
| **Header** | Connection state, device time and the appearance of the web UI (light, dark or system) |
| **Controls** | Display on or off, brightness |
| **Sleep** | Sleep schedule (from, until) and how long a tap wakes the display |
| **Views** | Choose an expression or random mode. The checkbox in the corner of each view decides whether random mode and tapping use it. At least one stays enabled. The slider sets how often random mode switches (5 seconds to 5 minutes). |
| **Colors** | Background and eye color with live preview, reset to defaults |
| **Settings** | Device name, timezone, 24 hour or AM/PM time format, password, WiFi information, firmware update, reboot, WiFi reset |

The accent color of the web UI follows the background color of the display.

Good to know:

- The small display shows colors paler than a phone or monitor. The default
  background is tuned so that it looks like Claude orange on the display. The
  button **Original Claude orange** sets the exact brand value `#D97757`.
- Blush, hearts, sparkles and the grumpy red are fixed accent colors. They do
  not change with the eye color.
- If you change the device name to `fred`, the address becomes
  `http://fred.local`.

## Sleep

Switch on the sleep schedule and set the times, for example 22:00 to 07:00.
The buddy gets the time from the internet (NTP) and uses the timezone from
the settings.

Going to sleep it shows sleepy eyes for a moment, then the display goes dark.
The device itself stays on and connected, so the web UI keeps working. A tap
wakes it for the configured time.

Switching the display off in the web UI works the same way.

## Firmware update

1. Download the newest `-ota.bin` file from the
   [releases page](https://github.com/PXWaldy/clawd-desk-buddy/releases).
   The `-factory.bin` file works as well.
2. In the web UI open **Settings**, choose the file under **Firmware update**
   and press **Upload**.
3. The buddy installs the update and restarts. Your settings are kept.

Files that are not a firmware for this board are rejected.

## Password

The password protection is off by default. Set a password in **Settings** to
protect the web UI and the API.

- The password is never stored. The device keeps only a random salt and a
  PBKDF2 hash.
- The connection is plain HTTP, like most devices in a home network. Somebody
  who can record the traffic in your WiFi can read the password while you log
  in. Do not reuse an important password here.
- Forgot the password? Do a factory reset: hold a finger on the display for
  15 seconds.

## Troubleshooting

| Problem | What to try |
|---|---|
| The display stays dark | Check that you have the **Touch** version of the board |
| `clawd.local` does not open | Use the IP address shown on the display after connecting, or look it up in your router. Some Android versions and guest networks do not support `.local` names. |
| The web flasher does not find the board | Use Chrome, Edge or Firefox 151 or newer on a computer and a USB cable that carries data, not only power. Safari, iPhone and iPad do not work. |
| The setup page does not open by itself | Open `http://192.168.4.1` while connected to `Clawd-Setup` |
| WiFi keeps dropping | The board has a small antenna. Move it closer to the router. The signal strength is shown in **Settings**. |
| Sleep schedule does nothing | It needs the time from the internet. The header of the web UI shows the device time once it is known. |

## Print and assembly

<!-- TODO: add the MakerWorld link and assembly photos -->

The printable files are published on MakerWorld (link follows). The set
contains:

- **Body**: the figure with the opening for the display
- **Lid**, in two variants. Print the one that fits your screws:
  - for **M2 x 16 cylinder head** screws
  - for original **Bambu Lab M2 x 20 countersunk** screws
- **Release tool**

> [!TIP]
> The release tool that comes with UniFi access points fits as well.

Mounting: the display sits in landscape in the figure, with the USB-C port
pointing to the right when you look at the figure from the front.

If your picture is upside down, change `DISPLAY_ROTATION` in
[include/config.h](include/config.h) from `1` to `3` and flash again.

## Development

```
include/config.h     Pins, rotation, eye geometry, colors and timings in one place
src/display.*        Panel driver, framebuffer, backlight
src/eyes.*           Expressions, idle animations, transitions, particles
src/behavior.*       Event queue: tap reaction, random mode, sleep states
src/touch.*          Touch controller and gestures
src/net.*            WiFi station, setup access point, mDNS, NTP
src/improv_serial.*  WiFi setup over USB (Improv Serial)
src/web.*            Web server, REST API, WebSocket
src/auth.*           Password hash and sessions
src/ota.*            Firmware update
src/settings.*       Persistent settings (NVS)
src/sleep.*          Sleep schedule
web/index.html       Web UI, packed into the firmware at build time
web/fonts/           Fonts of the web UI, packed into the firmware as well
flasher/             Web flasher page and its Cloudflare configuration
```

Everything that wants to influence the buddy posts an event to
`behavior::post()`. Touch and the web UI use it today, other inputs can be
added the same way.

The HTTP API is described in [docs/api.md](docs/api.md).

A serial console at 115200 baud helps while developing: `0` to `9` and `A` to
`N` select an expression, `r` switches random mode on, `t` simulates a tap. Note that
opening the serial port restarts the board.

### Releases

Pushing a tag like `v1.2.0` starts the release workflow. It builds the
firmware with that version, creates a GitHub release with the `-factory.bin`
and `-ota.bin` files and deploys the web flasher with the new firmware to
Cloudflare. The page, the manifest and the firmware are all served from
<https://pxwaldy.com/clawd-desk-buddy/>.

The flasher is a Cloudflare Worker that only serves static files
([flasher/wrangler.jsonc](flasher/wrangler.jsonc)). It is bound to the path
`pxwaldy.com/clawd-desk-buddy/*`, the rest of the domain is not touched.

One-time setup:

1. `pxwaldy.com` needs a proxied DNS record (orange cloud) in Cloudflare.
   Without one the address does not resolve and the Worker is never reached.
   As long as there is no real site at the root, a placeholder is enough:
   type `AAAA`, name `@`, content `100::`, proxied.
2. Create a Cloudflare API token (**Manage account > Account API tokens >
   Create Token > Custom token**) with exactly these permissions:

   | Scope | Permission | Access | Resources |
   |---|---|---|---|
   | Account | Workers Scripts | Edit | only your account |
   | Zone | Workers Routes | Edit | only the zone `pxwaldy.com` |

3. Add two secrets to the GitHub repository (**Settings > Secrets and
   variables > Actions**): `CLOUDFLARE_API_TOKEN` with the token and
   `CLOUDFLARE_ACCOUNT_ID` with the account ID from the Cloudflare dashboard.

To try the flasher page locally:

```
pio run
python3 scripts/build_flasher.py
cd flasher && npx wrangler@4.162.0 dev
```

Then open `http://localhost:8787/clawd-desk-buddy/`.

## Credits

- Display register values and touch controller facts for this board come from
  the official [Waveshare documentation and example package](https://docs.waveshare.com/ESP32-C6-Touch-LCD-1.47).
- Graphics: [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
- Web server: [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer)
  and [AsyncTCP](https://github.com/ESP32Async/AsyncTCP)
- JSON: [ArduinoJson](https://arduinojson.org)
- Fonts of the web UI: [Inter](https://rsms.me/inter/) and
  [JetBrains Mono](https://www.jetbrains.com/lp/mono/), both under the SIL
  Open Font License (see [web/fonts](web/fonts))
- Web flasher: [ESP Web Tools](https://esphome.github.io/esp-web-tools/) and
  [Improv Wi-Fi](https://www.improv-wifi.com)
- Inspiration: [clawd-mochi](https://github.com/yousifamanuel/clawd-mochi) for
  the idea of eyes with a web controller, and
  [Monitor-Buddy](https://github.com/IdefixRC/Monitor-Buddy) for the idea of a
  web flasher on this board. No code from these projects is used.

## Disclaimer

This is a fan project. It is not affiliated with, endorsed by or sponsored by
Anthropic. "Claude" and "Clawd" are trademarks of Anthropic.

## License

[MIT](LICENSE)
