# QindaLink mirror protocol 1

This page is the normative contract between `qindaqt-link-mirror` (desktop,
`src/apps/link_mirror`) and the QindaLink mirror server (phone,
`phone/mirror_server`). It covers how the desktop starts the server over ADB,
the byte stream between them, and the behaviour each side must keep. It is
separate from the TLS link [QindaLink protocol 1](qindalink-protocol-v1.md):
the mirror never uses the link connection, pairing keys or the link service.

The decisions behind it are in
[ADR-0433](../adr/0433-qindalink-phone-companion-architecture.md), and the
[architecture page](../architecture/qindalink.md#phone-apps-on-the-desktop)
describes the components.

<!-- AGENT-CONTRACT: the desktop codec (src/apps/link_mirror/src/mirror_protocol.cpp,
mirror_messages.cpp), the Java server (phone/mirror_server/.../Frames.java, Session.java)
and tests/apps/link_mirror/tst_mirror_framing.cpp implement exactly this page. Changing a
value changes all of them in one commit. -->

## Session setup

One desktop process mirrors one phone app. It runs these steps in order:

1. **Token.** The desktop draws 16 bytes from the system CSPRNG and writes
   them as 32 lowercase hex characters. The token is never logged.
2. **Class path.**
   - With `--server-jar FILE`, the desktop pushes the jar to
     `/data/local/tmp/qindalink-mirror-dev.jar` and uses that path.
   - Otherwise it uses the installed QindaLink APK in place, from
     `adb shell pm path org.qindaqt.link` (the APK compiles the same
     `phone/mirror_server` sources).
3. **Server.** The desktop starts the server as the `shell` user:

   ```
   adb -s <serial> shell CLASSPATH=<class path> app_process / \
       org.qindaqt.link.mirror.Server version=1 token=<token> [verbose=1]
   ```

4. **Socket.** The server listens on the abstract Unix socket
   `qindalink-mirror-<token>`.
5. **Forward.** The desktop runs
   `adb -s <serial> forward tcp:0 localabstract:qindalink-mirror-<token>` and
   reads the chosen port from its output.
6. **Connect.** The desktop connects to `127.0.0.1:<port>` and sends `hello`.
   ADB accepts a forwarded connection even while nothing listens on the
   phone, then closes it. Until `ready` arrives, the desktop therefore treats
   a closed connection as "server not listening yet" and retries every
   150 ms, for up to 10 s.
7. **Hello.** The server checks the token and the version. A peer whose
   first frame is not a valid `hello` with the right token is dropped, and the
   server keeps listening until its accept deadline.

The server serves exactly one session. When that connection closes for any
reason, the server releases the display and exits.

### Server arguments and exit codes

| Argument | Meaning |
| --- | --- |
| `version=1` | Required. Protocol version the desktop speaks. |
| `token=<hex>` | Required. 16–64 lowercase hex characters. |
| `verbose=1` | Debug lines on stdout. |
| `accept-timeout-ms=N` | Exit when no valid desktop connects in time. Default 20 000. |
| `probe` (alone) | Print the hidden-API report and exit. |

| Exit | Meaning |
| --- | --- |
| 0 | A session ran and ended, or `probe` found every required method |
| 2 | Bad arguments, or a version the server does not speak |
| 3 | `probe` found a required method missing |
| 4 | No valid desktop connected before the accept deadline |

The server writes `[qindalink-mirror] <level> <text>` lines to stdout. The
desktop repeats them with `--verbose`, and after a failure.

## Framing

Every frame, in both directions, is an 8-byte header followed by the payload.

| Offset | Size | Field | Rule |
| --- | --- | --- | --- |
| 0 | 1 | channel | `0x01` control, `0x02` video, `0x03` inventory |
| 1 | 1 | type | Message type within the channel |
| 2 | 2 | reserved | Must be zero |
| 4 | 4 | length | Unsigned big-endian payload length |

- **Length.** A control or inventory payload is 1 to 1 048 576 bytes (1 MiB).
  A video payload is 1 to 16 777 216 bytes (16 MiB).
- **Errors.** An unknown channel, non-zero reserved bytes or a length out of
  range is a protocol error. The receiver closes the connection without
  replying. The decoder outcomes are named `bad-channel`, `bad-reserved` and
  `bad-length`, in that checking order. A frame whose bytes have not all
  arrived is `needs-more`, which is not an error.
- **Unknown types.** A receiver ignores an unknown type on a known channel.
- **Bodies.** Control and inventory payloads are compact UTF-8 JSON objects.
  Receivers ignore unknown keys. Video packet payloads are binary.

## Control channel (`0x01`)

| Type | Direction | Name | Body |
| --- | --- | --- | --- |
| `0x01` | desktop → phone | `hello` | `version` (1), `token`, `client` |
| `0x02` | desktop → phone | `start` | `package`, `width`, `height`, `dpi`, `codec`, `bitrateBps`, `maxFps` |
| `0x03` | desktop → phone | `resize` | `width`, `height`, `dpi` |
| `0x04` | desktop → phone | `touch` | `action`, `pointerId`, `x`, `y`, `pressure` |
| `0x05` | desktop → phone | `mouse` | `action`, `x`, `y`, `buttons` |
| `0x06` | desktop → phone | `scroll` | `x`, `y`, `h`, `v` |
| `0x07` | desktop → phone | `key` | `action`, `keycode`, `meta`, `repeat` |
| `0x08` | desktop → phone | `text` | `text` |
| `0x09` | desktop → phone | `sync` | `{}`: request a keyframe |
| `0x0A` | desktop → phone | `stop` | `{}`: end the session |
| `0x81` | phone → desktop | `ready` | `version`, `sdk`, `release`, `model`, `missing` (list of absent required methods) |
| `0x82` | phone → desktop | `display` | `displayId`, `width`, `height`, `dpi`, `flags` |
| `0x83` | phone → desktop | `error` | `code`, `message`, `fatal` |

Field rules:

- **Sizes** are virtual-display pixels. The server clamps them to 64–8192 and
  rounds them down to what the encoder accepts (multiples of at least 8, and
  smaller still if the encoder rejects the size). It reports the real size in
  `display`, which follows every `start` and `resize`.
- **Density.** `dpi` is clamped to 72–960. The desktop sends
  `160 × device pixel ratio` unless `--dpi` is given, so 1 dp is one desktop
  pixel.
- **Codec.** `codec` is `h264` (the default) or `hevc`. `bitrateBps` is
  clamped to 0.5–100 Mb/s (default 8 Mb/s) and `maxFps` to 1–120 (default
  60).
- **Coordinates.** `x` and `y` are virtual-display pixels as JSON numbers.
- **Touch.** `touch.action` is `down`, `move`, `up` or `cancel`, with up to
  10 pointers distinguished by `pointerId`.
- **Mouse.** `mouse.action` is `hover`, `down`, `move` or `up`. `buttons`
  holds Android `MotionEvent.BUTTON_*` bits.
- **Scroll.** `scroll.v` is positive away from the user and `h` positive to
  the right, in wheel notches (Android `AXIS_VSCROLL` and `AXIS_HSCROLL`).
- **Keys.** `key.action` is `down` or `up`. `keycode` and `meta` are Android
  `KeyEvent` values. A held key repeats as `down` with a growing `repeat`.
- **Text.** The server types `text` through the virtual keyboard's key
  character map. Characters without a mapping are dropped, and the server
  logs only their count.
- **Errors.** After a fatal `error` the server closes the session. Codes:

| Code | Meaning |
| --- | --- |
| `unsupported-version` | The hello asked for a version the server does not speak |
| `api-missing` | A required hidden method is absent; the message names it |
| `no-launcher-activity` | The package has no launcher activity for user 0 |
| `bad-request` | A field has a value outside its contract |
| `bad-state` | `start` twice, or `resize` before `start` (not fatal) |
| `bad-message` | A control body is not the expected JSON |
| `internal` | Anything else; the message carries the exception summary |

## Video channel (`0x02`)

Only the phone sends on this channel.

- **`format` (`0x01`).** A JSON body with `codec`, `width` and `height`. It
  starts a stream generation. The desktop reopens its decoder and expects new
  parameter sets.
- **`packet` (`0x02`).** A binary body:

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 1 | flags: `0x01` config (SPS/PPS/VPS), `0x02` keyframe; other bits must be zero |
| 1 | 8 | presentation time, signed big-endian microseconds |
| 9 | ≥ 1 | Annex-B bitstream, exactly as MediaCodec produced it |

Rules:

- Each `start` and each effective `resize` begins a new generation. The order
  is: `format`, then a config packet, then a keyframe, then frames. Ordering
  on the single connection guarantees that no packet of an old generation
  follows a new `format`.
- The encoder repeats the last frame after 100 ms without changes, so a static
  screen still delivers about 10 frames per second.
- The keyframe interval is 10 s. The desktop sends `sync` when it needs a
  keyframe sooner, for example after a decoder fallback.

## Inventory channel (`0x03`)

| Type | Direction | Name | Body |
| --- | --- | --- | --- |
| `0x01` | desktop → phone | `list` | `iconSize` (16–512, default 96); optional `packages` filter |
| `0x81` | phone → desktop | `app` | `package`, `activity`, `label`, `png` (base64, optional) |
| `0x82` | phone → desktop | `end` | `count`, optional `error` |

The server sends one `app` per launchable package, de-duplicated, and then
`end`. An inventory failure ends with `end{count, error}` and does not end the
session. A mirror window asks for its own package only, at 128 px, for its
title and icon.

## Virtual display behaviour

- **Flags.** The server first tries `PUBLIC | OWN_CONTENT_ONLY |
  SUPPORTS_TOUCH | DESTROY_CONTENT_ON_REMOVAL | TRUSTED | OWN_DISPLAY_GROUP |
  ALWAYS_UNLOCKED | TOUCH_FEEDBACK_DISABLED | OWN_FOCUS |
  STEAL_TOP_FOCUS_DISABLED` (`0x17d49`). On `SecurityException` it drops the
  trusted group, then `PUBLIC`. The `display.flags` field reports the set that
  was accepted.
- **Phone keyboard.** The server sets the display's IME policy to hide.
  Desktop typing replaces the phone keyboard, and a text field on the mirror
  never opens the keyboard on the phone's own screen.
- **Power.** `OWN_DISPLAY_GROUP` gives the display its own wakefulness. The
  server holds a screen wake lock bound to the display id with
  `ACQUIRE_CAUSES_WAKEUP`, which wakes and keeps only that display group. If
  that call is unavailable, it injects `KEYCODE_WAKEUP` on the display
  instead. The lock is a binder of the server process, so it ends with the
  process.
- **Launch.** The server starts the package's first launcher activity for
  user 0 with `FLAG_ACTIVITY_NEW_TASK` and `setLaunchDisplayId`. If the app
  already has a task on the phone's screen, Android moves that task to the
  mirror display.
- **Release.** On session end the server detaches and stops the encoder,
  releases the wake lock and releases the display.
  `DESTROY_CONTENT_ON_REMOVAL` then ends the app's task on it. The server never
  force-stops a package. If the server dies, system_server releases the
  display through the death of the callback binder.

### Hidden framework methods

The server probes these by reflection at startup. They were verified on the
SM-S948U (Android 17, SDK 37, One UI).

| Need | Method | Required |
| --- | --- | --- |
| Display | `IDisplayManager.createVirtualDisplay(VirtualDisplayConfig, IVirtualDisplayCallback, IMediaProjection, String)` | yes |
| Display | `IDisplayManager.resizeVirtualDisplay(IVirtualDisplayCallback, int, int, int)` | yes |
| Display | `IDisplayManager.setVirtualDisplaySurface(IVirtualDisplayCallback, Surface)` | yes |
| Display | `IDisplayManager.releaseVirtualDisplay(IVirtualDisplayCallback)` | yes |
| Input | `IInputManager.injectInputEvent(InputEvent, int)`, `InputEvent.setDisplayId(int)` | yes |
| Input | `MotionEvent.setActionButton(int)` | no (mouse button events) |
| Launch | `IActivityManager.startActivityAsUserWithFeature(…, Bundle, int)`, else `startActivityAsUser` | yes |
| Launch | `IPackageManager.queryIntentActivities(Intent, String, long, int)`, else the `int` flags form | yes |
| Keyboard | `IWindowManager.setDisplayImePolicy(int, int)` | no |
| Power | `IPowerManager.acquireWakeLock(IBinder, int, String, String, WorkSource, String, int, IWakeLockCallback)`, `releaseWakeLock` | no |
| Inventory | `ActivityThread()` with a `ConfigurationController`, then `getSystemContext()` | inventory only |

The callback the display manager needs is a `java.lang.reflect.Proxy` for
`IVirtualDisplayCallback` whose `asBinder()` returns a plain `Binder`. The
server compiles against the public SDK only.

## Desktop command

```
qindaqt-link-mirror --serial <serial> --package <package> [options]
```

| Option | Meaning |
| --- | --- |
| `--server-jar FILE` | Push and run a development jar (see `phone/mirror_server/build-dev-jar.sh`) |
| `--adb PATH` | ADB client; default Portage `/usr/bin/adb` |
| `--size WxH` | Initial window size in logical pixels (default 432x900) |
| `--dpi N` | Virtual display density (default 160 × device pixel ratio) |
| `--codec h264\|hevc`, `--bitrate BPS`, `--max-fps N` | Encoder settings |
| `--decoder auto\|vaapi\|software` | Decoder path (default `auto`) |
| `--verbose` | Repeat server lines and decoder events |
| `--list` | Print `package<TAB>label<TAB>icon=WxH` for each launchable app and exit |
| `--grab FILE [--grab-after-frames N]` | Render offscreen, write one presented frame as PNG and exit |
| `--input STEP … --grab-after-input FILE` | After the first grab, run the steps and grab again |
| `--probe [--probe-seconds N]` | Print the decoder path, resolution, frame count and fps each second, then exit |

`--input` steps use window coordinates: `tap:X,Y`, `scroll:X,Y,NOTCHES`,
`text:STRING`, `key:NAME` (`back`, `enter`, `tab`, `escape`, `backspace`,
`up`, `down`, `left`, `right`) and `resize:WxH`. The steps are sent as
synthetic Qt events to the mirror window, so they take the same mapping path
as real input.

`--grab`, `--probe` and `--list` select the `offscreen` Qt platform unless
`QT_QPA_PLATFORM` is set, and never show a window. The window's app id is
`org.qindaqt.link.mirror.<package>`, and its title and icon come from the
inventory.

Exit status: 0 on a normal end, 1 when the session failed (the reason and the
last server lines are printed), 2 for bad arguments.

## Desktop decoding

- `auto` opens VA-API first (libavcodec with a VA-API hardware device; on
  qinda that is radeonsi on the RX 5700 XT), then software libavcodec.
- A path that fails to open is skipped for that generation.
- If VA-API fails while decoding, the desktop switches to software for the
  rest of the session and sends `sync` for a fresh keyframe.
- A software decode error drops that packet and keeps the decoder.
- An explicit `vaapi` or `software` choice never falls back.
- The VA-API path never falls back to software inside libavcodec, so `--probe`
  reports the path that actually decoded.
- Decoding runs on a worker thread. Only the newest picture is handed to the
  window, so a slow paint drops frames instead of adding latency.

## Desktop input mapping

- The frame is drawn aspect-preserving and centred in the window. Window
  positions map back through the same rectangle, and a held drag that leaves
  it is clamped to the display edge.
- The primary button drives one finger (`touch`). The secondary button and
  the mouse Back button send Android `BACK`, because a mirror display has no
  navigation bar.
- The wheel sends `scroll` at the pointer position.
- Printable typing without Ctrl, Alt or Meta is sent as `text`. Other keys
  and chords (Enter, Backspace, arrows, Ctrl+C and similar) are sent as `key`
  with Android meta state.
- Volume, power, camera and call keys are never mapped.
- Losing focus or closing the window cancels a held finger and releases held
  keys.
- Resizing is debounced by 250 ms. The new window size times the device pixel
  ratio becomes a `resize` request.

## Privacy and safety

- Neither side logs `text` bodies, inventory labels or package lists, or the
  session token. Server logs carry counts and lengths.
- The desktop removes only the forward it created and ends only the
  `adb shell` process it started.
- The server never force-stops apps, changes global settings or touches the
  phone's own display.

## Known limits

- The desktop drives a single finger. Multi-touch is in the protocol, but no
  desktop gesture produces it yet.
- Windows with `FLAG_SECURE` appear black in the stream.
- Audio is not forwarded (M3).
- Streaming stops when ADB drops. A visible window then shows the reason
  until it is closed; headless runs exit with status 1 and the reason.
