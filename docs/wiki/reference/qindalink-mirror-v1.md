# QindaLink mirror protocol 1

This page is the normative contract between `qindaqt-link-mirror` (desktop,
`src/apps/link_mirror`) and the QindaLink mirror server (phone,
`phone/mirror_server`). It covers how the desktop starts the server over ADB,
the byte stream between them, and the behaviour each side must keep. It is
separate from the TLS link [QindaLink protocol 1](qindalink-protocol-v1.md):
the mirror never uses the link connection, pairing keys or the link service.

The decisions behind it are in
[ADR-0433](../adr/0433-qindalink-phone-companion-architecture.md) and, for
decoding, [ADR-0435](../adr/0435-decode-qindalink-streams-with-ffmpeg.md). The
[architecture page](../architecture/qindalink.md#phone-apps-on-the-desktop)
describes the components.

<!-- AGENT-CONTRACT: the desktop codec (src/apps/link_mirror/src/mirror_protocol.cpp,
mirror_messages.cpp), the Java server (phone/mirror_server/.../Frames.java, Session.java)
and tests/apps/link_mirror/tst_mirror_framing.cpp implement exactly this page. Changing a
value changes all of them in one commit. -->

## Session setup

One desktop process mirrors one phone app. It runs these steps in order:

1. **Secret and suffix.** The desktop draws two values from the system
   CSPRNG:
   - a **secret** of 16 bytes, written as 32 lowercase hex characters;
   - a non-secret **socket suffix** of 8 bytes, written as 16 lowercase hex
     characters.

   The secret is never logged, and never placed in argv, a socket name or a
   forward spec.
2. **Class path.**
   - With `--server-jar FILE`, the desktop pushes the jar to the
     content-addressed path
     `/data/local/tmp/qindalink-mirror-<first 16 hex of its SHA-256>.jar`.
     It checks the file with `sha256sum` on the phone before and after the
     push. Two mirror windows never overwrite each other's jar, and an
     unchanged jar is not pushed again. Jars from older builds stay until
     `adb shell rm /data/local/tmp/qindalink-mirror-*.jar` removes them.
   - Otherwise the desktop uses the installed QindaLink APK in place, from
     `adb shell pm path org.qindaqt.link` (the APK compiles the same
     `phone/mirror_server` sources).
3. **Server.** The desktop starts the server as the `shell` user, without a
   pty (`-T`, so stdin is never echoed), and writes the secret plus a newline
   as the first line of the server's stdin:

   ```
   adb -s <serial> shell -T CLASSPATH=<class path> app_process / \
       org.qindaqt.link.mirror.Server version=1 socket=<suffix> [verbose=1]
   ```

4. **Socket.** The server reads the secret, then listens on the abstract Unix
   socket `qindalink-mirror-<suffix>`.
5. **Forward.** The desktop runs
   `adb -s <serial> forward tcp:0 localabstract:qindalink-mirror-<suffix>`.
   It takes the port from the last non-empty stdout line, and parses stderr
   separately.
6. **Connect.** The desktop connects to `127.0.0.1:<port>` and sends `hello`
   with the secret as `token`.
   - ADB accepts a forwarded connection even while nothing listens on the
     phone, then closes it. Until `ready` arrives, the desktop treats a
     closed connection as "server not listening yet" and retries every
     150 ms, for up to 10 s.
7. **Hello.** The server compares the token with the secret in constant time
   and checks the version.
   - A peer gets 5 s to send a valid `hello`. A peer that sends anything else,
     or nothing, is dropped.
   - After 3 dropped peers the server exits.
   - If no peer completes `hello` within the accept deadline (20 s from
     start), the process exits even if a read is still blocked.
8. **Keepalive.** After `hello` the desktop sends `ping` every 5 s. A greeted
   session that receives no frame for 30 s releases its display and exits.

The server serves exactly one session. When that connection closes for any
reason, the server releases the display and exits. A shell-uid server is
never left running without its desktop.

### Server arguments and exit codes

| Argument | Meaning |
| --- | --- |
| `version=1` | Required. Protocol version the desktop speaks. |
| `socket=<hex>` | Required. The non-secret socket suffix, 8–32 lowercase hex characters. |
| stdin, first line | Required. The secret, 32–64 lowercase hex characters (at most 128 bytes are read). |
| `verbose=1` | Debug lines on stdout. |
| `accept-timeout-ms=N` | Exit when no desktop completes `hello` in time. Default 20 000. |
| `probe` (alone) | Print the hidden-API report and exit; needs no stdin. |

| Exit | Meaning |
| --- | --- |
| 0 | A session ran and ended, or `probe` found every required method |
| 2 | Bad arguments, a missing or malformed secret on stdin, or a version the server does not speak |
| 3 | `probe` found a required method missing |
| 4 | No desktop completed `hello` before the accept deadline |
| 5 | Three peers failed `hello` |

The server writes `[qindalink-mirror] <level> <text>` lines to stdout. The
desktop repeats them with `--verbose`, and after a failure.

## Threat model

The mirror assumes a **single-user desktop**. The ADB connection grants
shell-uid access to the phone.

- **What the secret protects.** The secret keeps other parties from driving a
  session. That covers other local users of the desktop, who can reach the
  forwarded `127.0.0.1` port and read `adb forward --list` and other users'
  `/proc/<pid>/cmdline`. It also covers apps on the phone that might connect
  to the abstract socket, whose name is not secret. Neither can complete
  `hello` without the secret, which travels only on the server's stdin and
  inside `hello`.
- **What it cannot stop.** A process running as the **same desktop user** is
  inside the trust boundary. It can talk to the ADB server and run
  `adb shell` itself, which already gives it everything the mirror server can
  do. It can also read the mirror process's memory where ptrace is allowed.
  The secret does not defend against it, and nothing in this design tries
  to.
- **Untrusted video.** The phone side of the stream is treated as untrusted.
  [ADR-0435](../adr/0435-decode-qindalink-streams-with-ffmpeg.md) confines
  bitstream parsing to the per-app mirror process. It also caps frame sizes,
  stream dimensions and the decoder backlog.
- **Phone-wide input.** The server injects only allowlisted keys, and only
  with the mirror display's id. Coordinates are clamped to that display, so
  desktop input cannot press global keys such as POWER, HOME, ASSIST or
  volume.

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
| `0x0B` | desktop → phone | `ping` | `{}`: keepalive, sent every 5 s after `ready` |
| `0x81` | phone → desktop | `ready` | `version`, `sdk`, `release`, `model`, `missing` (list of absent required methods) |
| `0x82` | phone → desktop | `display` | `displayId`, `width`, `height`, `dpi`, `flags` |
| `0x83` | phone → desktop | `error` | `code`, `message`, `fatal` |
| `0x84` | phone → desktop | `app-exited` | `reason` (`no-tasks`): the app left the mirror display; the server then closes the session |
| `0x85` | phone → desktop | `screen-state` | `state`: `off` or `on`. The phone's own screen changed while the mirror shares its power group |

Field rules:

- **Stream caps.** A `display` or `format` size beyond 8192 pixels per side or
  64 000 000 pixels is invalid, and the desktop ignores it. The desktop
  applies the same caps to every decoded picture.
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
- **Coordinates.** `x` and `y` are virtual-display pixels as JSON numbers. The
  server clamps them to the current display, and `NaN` becomes 0.
- **Touch.** `touch.action` is `down`, `move`, `up` or `cancel`, with up to
  10 pointers distinguished by `pointerId`.
- **Mouse.** `mouse.action` is `hover`, `down`, `move` or `up`. `buttons`
  holds Android `MotionEvent.BUTTON_*` bits. Only primary, secondary,
  tertiary, back and forward pass.
- **Scroll.** `scroll.v` is positive away from the user and `h` positive to
  the right, in wheel notches (Android `AXIS_VSCROLL` and `AXIS_HSCROLL`).
- **Keys.** `key.action` is `down` or `up`. `keycode` and `meta` are Android
  `KeyEvent` values. A held key repeats as `down` with a growing `repeat`.
  - The server injects only an **allowlist**: BACK, digits, letters, DPAD,
    TAB, SPACE, ENTER, DEL, punctuation, MENU, page keys, ESCAPE,
    FORWARD_DEL, MOVE_HOME/END, INSERT, F1–F12 and NUMPAD_ENTER.
  - Other keycodes are dropped and logged without their value. That includes
    POWER, SLEEP, WAKEUP, HOME, ASSIST, APP_SWITCH, volume, call, camera,
    brightness and META.
- **Text.** The server types `text` through the virtual keyboard's key
  character map. Characters without a mapping are dropped, and the server
  logs only their count.
- **Screen state.** `screen-state` is sent only when the mirror display shares
  the phone's power group (display group 0).
  - The server polls `IPowerManager.isInteractive()` every 500 ms and sends
    each change.
  - On `on` it also requests a keyframe, so the picture resumes at once.
  - The session stays open while the phone screen is off.
- **Errors.** After a fatal `error` the server closes the session. Codes:

| Code | Meaning |
| --- | --- |
| `unsupported-version` | The hello asked for a version the server does not speak |
| `api-missing` | A required hidden method is absent; the message names it |
| `no-launcher-activity` | The package has no launcher activity for user 0 |
| `bad-request` | A field has a value outside its contract |
| `bad-state` | `start` twice, or `resize` before `start` (not fatal) |
| `bad-message` | A control body is not the expected JSON |
| `display-asleep` | The mirror display cannot be kept awake without waking the phone's own screen, and it is not awake now |
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
- The server asks MediaCodec to prepend SPS/PPS to every IDR frame
  (`KEY_PREPEND_HEADER_TO_SYNC_FRAMES`). The desktop also replays the
  generation's config packet into any decoder it opens mid-stream.

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
- **Power.** The server reads the display's `DisplayInfo.displayGroupId`.
  - **Own group.** Where `OWN_DISPLAY_GROUP` is honoured and the display has
    its own group, the server holds a screen wake lock bound to the display id
    with `ACQUIRE_CAUSES_WAKEUP`. That lock wakes and keeps only that group.
  - **Group 0.** On the SM-S948U (SDK 37, One UI) the flag is accepted, but
    the display stays in display group 0, and a lock bound to it is recorded
    as `powerGroupId=0`. So in group 0 the server takes **no** wake lock, and
    the mirror follows the phone's own power state. It starts only while the
    phone is awake, and otherwise fails with `error{display-asleep}`.
  - **Pause on screen-off.** This is the user's decision of 2026-11-02. In
    group 0 the mirror pauses while the phone's screen is off and reports it
    with `screen-state`. The server never holds the phone's main screen
    awake, and never turns its panel off.
  - There is **no** wake-key or global-wakeUp fallback, because those can
    wake the phone's own screen.
  - Without the per-display lock, the session continues only if the display is
    already interactive (in group 0: if the phone's own display is).
    Otherwise it ends with `error{display-asleep}`.
  - The lock is a binder of the server process, so it ends with the process.
- **Launch.** The server starts the package's first launcher activity for
  user 0 with `FLAG_ACTIVITY_NEW_TASK` and `setLaunchDisplayId`. If the app
  already has a task on the phone's screen, Android moves that task to the
  mirror display.
- **App exit.** After launch the server polls
  `IActivityTaskManager.getAllRootTaskInfosOnDisplay` once a second.
  - Once the app has been seen on the display, two polls in a row with no
    root task that has a top activity count as an exit. The server then sends
    `app-exited` and ends the session.
  - The desktop window shows "App closed on phone" and closes after 2 s.
    Headless runs exit with status 3.
  - Without that method, detection is off and the display just stays empty.
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
| App exit | `IActivityTaskManager.getAllRootTaskInfosOnDisplay(int)` | no |
| Power | `IPowerManager.acquireWakeLock(IBinder, int, String, String, WorkSource, String, int, IWakeLockCallback)`, `releaseWakeLock`, `isDisplayInteractive(int)` | no |
| Power | `IDisplayManager.getDisplayInfo(int)` → `DisplayInfo.displayGroupId` | no (unknown counts as group 0) |
| Screen state | `IPowerManager.isInteractive()`, else `isDisplayInteractive(0)` | no (no `screen-state` messages) |
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
| `--list-json` | Print one JSON line `{"apps":[{"package","label","icon":"<base64 PNG or empty>"}]}` and exit; the QindaLink app reads this (needs `--serial`, no `--package`) |
| `--grab FILE [--grab-after-frames N]` | Render offscreen, write one presented frame as PNG and exit |
| `--input STEP … --grab-after-input FILE` | After the first grab, run the steps and grab again |
| `--probe [--probe-seconds N]` | Print the decoder path, resolution, frame count and fps each second, then exit |

`--input` steps use window coordinates: `tap:X,Y`, `scroll:X,Y,NOTCHES`,
`text:STRING`, `key:NAME` (`back`, `enter`, `tab`, `escape`, `backspace`,
`up`, `down`, `left`, `right`) and `resize:WxH`. The steps are sent as
synthetic Qt events to the mirror window, so they take the same mapping path
as real input.

`--grab`, `--probe`, `--list` and `--list-json` select the `offscreen` Qt platform unless
`QT_QPA_PLATFORM` is set, and never show a window. The window's app id is
`org.qindaqt.link.mirror.<package>`, and its title and icon come from the
inventory.

Exit status:

| Status | Meaning |
| --- | --- |
| 0 | Normal end, including a window closed after "App closed on phone" |
| 1 | The session failed; the reason and the last server lines are printed |
| 2 | Bad arguments |
| 3 | A headless run whose app closed on the phone before the run finished |

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
- **Keyframe requests.** Every `sync` the desktop sends goes through one
  throttle with a 1 s minimum gap. That covers backlog drops, decoder
  fallback and screen-on. A persistently slow decoder therefore cannot turn
  the backlog gate into a keyframe request loop.
- **Backlog.** At most 8 packets wait for the decoder. Past that, non-key
  packets are dropped until the next keyframe, and the desktop sends `sync`.
  Config and key packets always pass. A session start can trigger one such
  `sync` while VA-API opens.

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
- **Paused (phone screen off).** After `screen-state off` the window keeps the
  last frame, dimmed, under the notice "Phone screen is off — wake your phone
  to continue".
  - Held fingers and keys are released, and further input is dropped, not
    queued.
  - The pause ends on `screen-state on`, or on the first keyframe that
    arrives after `off`.
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
- **Phone must stay awake.** On builds that keep the mirror display in display
  group 0 (the SM-S948U on SDK 37), mirroring needs the phone awake. When the
  phone's screen times out, the window shows the paused notice until the
  phone is woken. This is by design (user decision).
- Audio is not forwarded (M3).
- Streaming stops when ADB drops. A visible window then shows the reason
  until it is closed; headless runs exit with status 1 and the reason.
