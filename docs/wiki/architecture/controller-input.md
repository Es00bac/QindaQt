# Controller desktop input

**Settings → Input → Controllers** configures Xbox, DualSense, other PlayStation, Nintendo and
other SDL-mapped game controllers. Family defaults are available before hardware
is connected; a connected controller gets its own remembered profile.
The page selects a connected controller initially and labels remembered offline
profiles as disconnected. Serial-backed identity is shared across USB and
Bluetooth; changing transport keeps the same preferences. A serial-less device
keeps its GUID identity. Existing matching legacy profiles are migrated on discovery.

## DualSense controls

DualSense and DualSense Edge use a separate `dualsense` family with the requested
layout. Other controller families retain the defaults in the next section.

| Control | Default |
| --- | --- |
| Create / Options | Escape |
| D-pad up / left / right / down | Ctrl+K / Ctrl+C / Ctrl+J / Enter |
| L3 / R3 | Previous / next window |
| PS | Open application launcher |
| Triangle | Maximize / restore the current window or focused container tile |
| Circle | Window overview |
| Square | Hold for voice commands; release to finish |
| Cross | Hold for voice dictation; release to insert |
| Mute | No desktop action |
| Touchpad click | Toggle gyro mouse |
| R1 / L1 | Left / right mouse button |
| R2 / L2 | Left / right mouse button held with Meta+Shift |

The gyro toggle changes the saved `gyro` preference once per press, publishes a
new revision, and works while gyro is off. Holding or releasing the button does
not toggle again. A failed save leaves the preference unchanged. Suppressed
input never toggles gyro; resuming still requires neutral controls. Controllers
without a gyro ignore this action. Gyro remains off in fresh defaults.

Mouse-chord actions press Meta and Shift before mouse-down and keep them held
until mouse-up. Shared controller keys/buttons use reference counts. Suppression,
disconnect, disable and unload release all owned input. Voice commands use
Voice1 `StartCommand`; dictation uses `StartDictation`. Both finish on release
and cancel when desktop control is suppressed.
Fresh holds can start from Voice1 Idle or Error, matching the provider's own
admission rule. They start a new capture and never retry an earlier utterance.

Existing saved bindings and pointer, scroll, touchpad and sensitivity preferences
are preserved when the new defaults are introduced. Older saved DualSense rows
are reclassified from `playstation` without changing their stable IDs or config.
Reset uses the device's factory family defaults. Applying the requested button
layout to existing devices is an explicit Controllers1 binding patch.

## Other controller defaults

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
maximize, close, workspace switching, voice commands, gyro toggle, modified mouse
clicks and a keyboard chord. Button names use
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

Runtime state, endpoints, SDL handles, delivery and Settings models stay on their
owning Qt GUI thread. One owned worker performs read-only process inspection at
normal scheduling priority; it neither follows FD targets nor touches input or
SDL. Periodic requests coalesce. Initial observation, device changes and
open/close notifications suppress desktop input until a current scan completes;
an invalidated result cannot restore authority. This transient state reports
`checking-game`. Periodic polling retains the last observation while its scan
runs, so held controls do not drop once a second. Destruction interrupts and
joins the worker before unloading plugin code
([ADR-0397](../adr/0397-observe-controller-consumers-off-the-compositor-thread.md)).
The SDL runtime borrows a sink that outlives it; each held
token must be released on suppression/disconnect/destruction. Voice1 dictation and command requests
use its exact unique bus owner and current revision, with no replay after an
owner replacement. [ADR-0347](../adr/0347-controller-desktop-integration.md)
records the dependency and priority decision;
[ADR-0426](../adr/0426-dualsense-desktop-layout-and-held-actions.md) records the
DualSense family, gyro persistence and held action contract.
Qt method-return messages do not supply the service owner through `service()`.
Controller dictation resolves the activated Voice1 name through the bus and reads
that unique owner before starting the capture.

## Focused verification

`src/controllers/standalone` builds the controller module and plugin against the
installed QindaQt KWin SDK. It does not compile the desktop or KWin.

- `qindaqt.controller-policy`: profile defaults, atomic hostile-patch refusal,
  save/load round trip, motion bounds, Steam and pre-existing device consumers.
- `qindaqt.controller-input-state`: modifier/mouse event ordering, shared-trigger
  modifiers and duplicate mouse holds.
- `qindaqt.controller-settings`: family/default display, available buttons/actions,
  edits gated by authority, readback, Steam status, owner loss.
- `qindaqt.controller-runtime`: SDL virtual DualSense layout, both voice actions, trigger chords,
  gyro press/hold/release and persisted preference, family-correct reset,
  stick/touchpad/gyro motion, suppression releases, neutral resume, Steam startup.
- `qindaqt.controller-observation`: a blocked scan leaves the event loop live,
  coalesced requests, invalidated-result rejection, normal scheduling and joined
  interrupted teardown.
- `qindaqt.controller-bus`: private out-of-process provider, change-during-read
  convergence, mutation completion after readback, and dictation/command start, release and cancellation.

Physical controller and USB microphone qualification is recorded separately;
passing virtual fixtures does not claim that hardware was connected.

See [Input settings](../apps/input-settings.md) and
[voice input](voice-input.md).

## Bounded repair packaging

The r12 controller repair compiles only `qindaqt-settings`, its Input QML plugin,
and the controller plugin. Its Portage recipe starts from the Manifest-pinned,
signed r11 package image and replaces those compiled artifacts through CMake.
Unchanged desktop programs keep their existing binary content. This is a bounded
ABI-compatible repair over the same Qt/KWin dependencies, not a general release
recipe: later changes outside these modules require the ordinary source build.
The result is a complete new signed desktop package with normal Portage ownership.

The r32 frame-timing repair uses the same bounded mechanism over the exact signed
r31 image, compiling only the controller plugin with the installed SDK. It
replaces that plugin under a fresh filename so Qt's retained old library mapping
does not prevent live adoption. All other base-image files are verified against
their ledger. A normal full source release continues to use the standard plugin
filename; Controllers1 and saved profiles do not change.

### October 17 frame-timing acceptance

Source `b331c826c` is integrated and the signed r32 repair is Portage-installed
on the laptop. The original compositor and shell processes stayed alive while
only the controller plugin was reloaded. Controllers1 responds, its observation
thread uses normal scheduling, and the saved controller file is byte-identical.
All 2,374 installed files/symlinks match the authenticated package image; its
other 2,652 base-image paths retain their r31 contents and modes.

The initial twelve-second DRM/scheduler trace caught twelve 28–29-refresh gaps
(up to 483 ms), aligned with 438–454 ms controller scans on the compositor
thread. The final twelve-second sample has a 16.667 ms median frame-delivery
interval, a 33.355 ms maximum, a 10.903 ms maximum compositor CPU run, and no
CPU run above 100 ms. The controller disconnected between those samples, so
the live comparison alone does not establish connected-controller playback
acceptance. The blocked-scanner fixture directly verifies the threading repair,
and SDL virtual fixtures retain controller/Steam/neutral-resume coverage.

The focused standalone build and five controller CTest groups pass (28 QtTest
cases, zero failures/skips). Strict MkDocs and local link/navigation validation
pass. The signed artifact is 63,313,920 bytes, SHA-256
`83637df1d16e3af764a4a87027093f1b80ec5475682c5cfc3d2b08ec6b84f5ca`.
