# Controller desktop input

**Settings → Input → Controllers** configures Xbox, PlayStation, Nintendo and
other SDL-mapped game controllers. Family defaults are available before hardware
is connected; a connected controller gets its own remembered profile.
The page selects a connected controller initially and labels remembered offline
profiles as disconnected. Serial-backed identity is shared across USB and
Bluetooth; changing transport keeps the same preferences. A serial-less device
keeps its GUID identity. Existing matching legacy profiles are migrated on discovery.

## Desktop controls

| Control | Default |
| --- | --- |
| Left stick | Mouse pointer |
| Right stick | Scroll |
| South face button / left stick click / right trigger | Left mouse button |
| East face button | Escape / back |
| West face button | Enter / accept |
| North face button | Window overview |
| Share / Create / View / Back / Minus | Hold to dictate; release to insert |
| Left / right shoulder | Previous / next window |
| Menu / Options / Plus | Launcher |
| Guide / PS / Home | Show desktop |
| D-pad | Arrow keys |
| Right stick click / left trigger | Right mouse button |
| Share / Capture / extra button | Screenshot |
| PlayStation touchpad click | Left mouse button |

Every available button can be reassigned to an enumerated action, including
maximize, close, workspace switching and a keyboard chord. Button names use
the controller family's labels and physical face-button positions. Dictation
binds directly to Voice1 and does not change the keyboard shortcut. Stick choice,
speed and dead zone are configurable. Supported controller touchpads move the
mouse; supported gyro sensors can be enabled with adjustable sensitivity and are
off by default. The other stick scrolls. Setting pointer stick Off disables stick
mouse and scrolling while keeping buttons, touchpad and gyro available.

## Games and session ownership

Steam always takes priority while running. The desktop plugin releases its SDL
handles until Steam exits. A game or other same-user process holding a related
input/js/hidraw node suppresses that controller's desktop output, including when
the game opened it before the desktop discovered it. Fullscreen apps, locked
screens and inactive sessions also suppress output. Current buttons and keys are
released, and controller-owned dictation is canceled. After suppression, controls
must be released and sticks centered before desktop actions resume.

The plugin never grabs controller nodes, never creates a virtual gamepad, never
remaps input delivered to games, and does not change Steam configuration.
Controller Sony touchpads have their duplicate libinput route disabled while
the plugin is loaded, so SDL is the one desktop touchpad owner. Original enabled
state is restored on unload.

## DualSense microphone

Connect the controller by USB, open the Audio settings link, and select its
capture device as the default input. Voice dictation uses that default microphone.
Availability comes from the kernel and Audio1 device list; Bluetooth microphone
audio is not advertised as supported. The feature does not install microphone
drivers or create a second audio service.

## Boundaries and persistence

`src/controllers` owns the public policy and the compositor plugin. Settings
models depend on its public value contract and a borrowed asynchronous
`ControllerPort`; they never read SDL, process handles or profile files. The
profile store atomically saves `qindaqt/controllers.json` under XDG config with
owner-only permissions. It stores no credentials and invokes no shell commands.

`org.qindaqt.Controllers1` on `/org/qindaqt/Controllers1` exports:

| Method | Contract |
| --- | --- |
| `GetSnapshot() -> s` | Version-1 JSON: schemaVersion, revision (decimal string), steam, error, controllers |
| `Apply(s id, s patch, t expectedRevision) -> s` | Result JSON: ok, reason, revision; ≤8 KiB atomic patch; exact revision |
| `Reset(s id, t expectedRevision) -> s` | Save validated defaults through the same mutation boundary |
| `Changed(t revision)` | Rediscover authoritative snapshot |

Snapshots are at most 256 KiB with at most 64 profiles and 32 connected devices.
Fields include id/name/family, connection/capability truth, current suppression
reason and config. Config has enabled, pointerStick, pointerSpeed, deadzone,
touchpad, gyro, gyroSpeed and a bounded button-to-action/shortcut map. Unknown
keys, out-of-range numbers, invalid buttons/actions and multi-chord shortcuts
are rejected without changing any setting. Settings discards replies from an old
service generation and clears edit authority on owner loss.
Edits remain busy until a fresh post-reply snapshot arrives. A change signal
received during a snapshot queues another read, so suppression/reconnect changes
cannot strand the next edit on an old revision. Interactive controls rebind to
confirmed settings after a gesture and when controller selection changes.

All runtime, endpoint, SDL handles, delivery and Settings models stay on their
owning Qt GUI thread. The SDL runtime borrows a sink that outlives it; each held
token must be released on suppression/disconnect/destruction. Voice1 requests
use its exact unique bus owner and current revision, with no replay after an
owner replacement. [ADR-0347](../adr/0347-controller-desktop-integration.md)
records the dependency and priority decision.
Qt method-return messages do not supply the service owner through `service()`.
Controller dictation resolves the activated Voice1 name through the bus and reads
that unique owner before starting the capture.

## Focused verification

`src/controllers/standalone` builds the controller module and plugin against the
installed QindaQt KWin SDK. It does not compile the desktop or KWin.

- `qindaqt.controller-policy`: profile defaults, atomic hostile-patch refusal,
  save/load round trip, motion bounds, Steam and pre-existing device consumers.
- `qindaqt.controller-settings`: family/default display, available buttons/actions,
  edits gated by authority, readback, Steam status, owner loss.
- `qindaqt.controller-runtime`: SDL virtual DualSense buttons, dictation action,
  stick/touchpad/gyro motion, suppression releases, neutral resume, Steam startup.
- `qindaqt.controller-bus`: private out-of-process provider, change-during-read
  convergence, mutation completion after readback, and dictation start/release.

Physical controller and USB microphone qualification is recorded separately;
passing virtual fixtures does not claim that hardware was connected.

See [Input settings](../apps/input-settings.md) and
[voice input](voice-input.md).
