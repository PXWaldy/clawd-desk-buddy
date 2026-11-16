# HTTP API

The web UI uses this API. You can use it as well, for example from a script
or a home automation system. All paths are relative to the device address,
for example `http://clawd.local`.

Requests and responses are JSON. Changing requests answer with
`{"ok": true}` or with HTTP 400 and `{"ok": false, "error": "..."}`.

## State

### `GET /api/state`

Returns the complete state.

| Field | Type | Meaning |
|---|---|---|
| `name`, `version` | string | Firmware name and version |
| `display_on` | bool | Display switch |
| `asleep` | bool | True while the display is dark (schedule or switch) |
| `brightness` | number | 0 to 100 percent |
| `expression` | string | Current expression id |
| `random` | bool | Random mode |
| `interval` | number | Random interval in seconds |
| `expressions` | array | All expressions with `id`, `name` and `enabled` |
| `bg`, `eye` | string | Colors as `#rrggbb` |
| `sleep_enabled` | bool | Sleep schedule on or off |
| `sleep_from`, `sleep_to` | string | Times as `HH:MM` (24 hour format) |
| `wake_seconds` | number | How long a tap wakes the display |
| `time` | string | Device time as `HH:MM`, empty until the clock is set |
| `clock_24h` | bool | Time format of the web UI |
| `timezone` | string | Timezone name |
| `password_enabled` | bool | Password protection |
| `wifi` | object | `connected`, `setup`, `ssid`, `ip`, `rssi`, `hostname`, `saved_ssid` |
| `defaults`, `accents` | object | Default and accent colors |

Expression ids: `normal`, `happy`, `in_love`, `excited`, `curious`,
`surprised`, `suspicious`, `sleepy`, `sad`, `grumpy`, `bored`, `shy`, `wink`,
`thinking`, `confused`, `dizzy`, `content`, `vibing`, `cool`, `ko`, `sweat`,
`money`, `touched`, `starstruck`.

### `POST /api/state`

Changes one or more values. Send only the fields you want to change.

| Field | Example | Notes |
|---|---|---|
| `display_on` | `true` | |
| `brightness` | `80` | 0 to 100 |
| `expression` | `"happy"` | Must be enabled. Switches random mode off. |
| `random` | `true` | |
| `interval` | `30` | 5 to 300 seconds |
| `enabled` | `["normal", "happy"]` | List of enabled expression ids, at least one |
| `bg`, `eye` | `"#e05500"` | |
| `sleep_enabled` | `true` | |
| `sleep_from`, `sleep_to` | `"22:00"` | 24 hour format |
| `wake_seconds` | `30` | 5 to 600 |
| `clock_24h` | `false` | |
| `timezone` | `{"name": "Europe/Berlin", "posix": "CET-1CEST,M3.5.0,M10.5.0/3"}` | Name for display, POSIX rule for the clock |
| `hostname` | `"clawd"` | a-z, 0-9 and `-` |

Example:

```bash
curl -X POST -H "Content-Type: application/json" -d '{"expression":"happy"}' http://clawd.local/api/state
```

### `POST /api/colors/reset`

Sets background and eye color back to the defaults.

## WebSocket

`ws://<device>/ws` sends the complete state (same JSON as `GET /api/state`)
after connecting and whenever something changes, also after a tap on the
figure. Text messages sent to the socket are handled like `POST /api/state`.

## WiFi

| Request | Meaning |
|---|---|
| `GET /api/wifi/scan` | Returns `{"scanning": bool, "networks": [{"ssid", "rssi", "secure"}]}`. Add `?start=1` to start a new scan. |
| `POST /api/wifi` | Body `{"ssid": "...", "pass": "..."}`. Tries to connect. The credentials are saved only if the connection works. |
| `GET /api/wifi/status` | `{"result": "none" \| "trying" \| "ok" \| "failed", "connected", "ip", "hostname"}` |
| `POST /api/wifi/reset` | Forgets the network and opens the setup network |

## System

| Request | Meaning |
|---|---|
| `POST /api/reboot` | Restarts the device |
| `POST /api/update` | Firmware update. Send the `.bin` file as `multipart/form-data`. The device restarts after a successful update. |

```bash
curl -F "firmware=@clawd-desk-buddy-1.0.0-ota.bin" http://clawd.local/api/update
```

## Password protection

When a password is set, every request to `/api/...` and the WebSocket need a
session cookie. Without it the device answers with HTTP 401.

| Request | Meaning |
|---|---|
| `GET /api/auth` | `{"enabled": bool, "authorized": bool}`. Always reachable. |
| `POST /api/login` | Body `{"password": "..."}`. Sets the session cookie `clawd_session`. Answers 401 for a wrong password and 429 if attempts come too fast. |
| `POST /api/logout` | Ends the session |
| `POST /api/password` | Body `{"password": "..."}` sets or changes the password (4 to 64 characters). An empty password removes the protection. All other sessions end. |

Sessions are kept in memory, so a restart of the device logs everybody out.

```bash
curl -c cookies.txt -X POST -H "Content-Type: application/json" -d '{"password":"secret"}' http://clawd.local/api/login
```

```bash
curl -b cookies.txt http://clawd.local/api/state
```
