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
| Right / left stick | Pointer / scrolling |
| Create | Cycle Desktop / Navigation pages |
| Options | Escape |
| D-pad up / left / right / down | Ctrl+K / Ctrl+C / Ctrl+J / Enter |
| L3 / R3 | Previous / next window |
| PS | Open application launcher |
| Triangle | Maximize / restore the current window or focused container tile |
| Circle | Window overview |
| Square | Hold for voice commands; release to finish |
| Cross | Hold for voice dictation; release to insert |
| Mute | Unassigned; retains microphone mute |
| Touchpad click / contact | Left click / gyro while touched |
| R1 / L1 | Left / right mouse button |
| R2 / L2 | Meta + left click / Meta+Shift + right click; hold the movement layer |

Desktop is the blue main page. Holding R2 keeps Meta and left click down and
selects the orange Movement layer. Its D-pad nudges the pointer/drag by ten
logical pixels, repeating after 250 ms and then every 60 ms through the existing
window/container pointer route. L2 holds Meta+Shift and right click with the same
layer. Releasing a modifier restores the base page without changing its saved
selection. Shared keys/clicks use reference counts, so overlapping holds cannot
release one another early.

Create cycles only the two ordinary pages. Mute remains unassigned because the
[Linux driver](https://kernel.googlesource.com/pub/scm/linux/kernel/git/stable/linux-stable/+/master/drivers/hid/hid-playstation.c)
also toggles the hardware microphone. Navigation is purple: D-pad arrows,
Triangle Enter, Circle Tab, L3 Backspace, Options Escape, R2 Shift and L2 Ctrl with the green
Editing layer. That layer maps D-pad up/down/left/right to A/X/C/V and Triangle /
Circle to Z/Y: with L2 held these are select all, cut, copy, paste, undo and redo.
Holding R2 too adds Shift, including Ctrl+Shift+C/V for terminals. Base layer
activators and modifier-only holds precede simultaneous letter presses. Held
modifier/gyro inputs survive layer transitions; other old-map holds release and
must be physically released before acquiring a new meaning.

Touchpad contact is independent of its physical click. Any finger enables
`hold-gyro`; lifting the last contact ends this temporary sensor enable without
saving or toggling the persistent gyro preference. Touchpad motion, gyro and the
pointer stick combine. A finger handoff reanchors touchpad motion to prevent a
cursor jump. Gyro sensitivity remains editable with always-on gyro disabled.
`toggle-gyro` remains available as a configurable action: press saves once,
hold/release never repeat it, and failed saves preserve the preference.

Settings separates the page being edited from the active profile. Editing a
layer does not activate it. Use profile activates an ordinary page; hardware
holds select temporary layers. Each page has a name, RGB color and its own map;
temporary layers are skipped when cycling. Cycle profiles assigns one chosen
button on every page. Color writes follow the effective page while desktop
control is active; unsupported/failed LEDs are reported without disabling valid
bindings. Suppression never repeatedly writes a game's LED.

Every button and contact can use a built-in action, arbitrary key/mouse
combination, Unicode text, a saved macro, page change or temporary layer. The
editor supports explicit modifiers, modifier-only holds, up to eight named or
recorded keys (including Esc/Tab) and five mouse buttons. Text runs once per
press, preserves line breaks and does not replace the clipboard. The shared
[macro library](input-actions.md) supports the same inputs, ordered holds,
movement, waits and reusable keyboard/mouse triggers.

Voice commands use Voice1 `StartCommand`; dictation uses `StartDictation`. Both
finish on release and cancel when desktop control is suppressed. Fresh holds
start from Idle or Error and never retry an old utterance. Suppression,
disconnect, disable, page change and unload release owned input and cancel text
and macros from that controller. Persistent page changes require neutral
buttons/sticks/contact before resuming; held layers do not pause pointer/gyro.

Version-1 saved maps migrate into a single Desktop page with device IDs,
bindings and motion preferences intact. Factory defaults apply to new controllers
and Reset; existing devices receive the owner's new preset through an explicit
Controllers1 patch after installing the new plugin. Other families keep their
existing defaults.

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
handles until Steam exits. Locked screens and inactive sessions also suppress
output. Fullscreen status alone does not suppress input, so browser video and
other fullscreen desktop applications retain controller control.

Another same-user process holding a related input/js/hidraw node pauses that
controller (`game`) only while it is part of the application that owns the
focused window **and** is not a passive web-engine host
([ADR-0428](../adr/0428-pause-controller-desktop-input-only-for-the-focused-application.md),
which supersedes ADR-0347's any-open rule). This includes a game that opened the
node before the desktop discovered it. A background holder suppresses nothing, so
a game in another window, a browser or a daemon holding the pad never turns
desktop control off. Current buttons and keys are released, and
controller-owned dictation is canceled, whenever control pauses. After a pause,
controls must be released and sticks centered before desktop actions resume.

A **passive web-engine host** is a holder whose mappings include QtWebEngine
(`libQt6WebEngineCore.so`), Gecko (`libxul.so`), CEF (`libcef.so`) or the ICU data
file that Chromium and Electron map (`icudtl.dat`). Their gamepad service opens
every pad once any page touches the Gamepad API and keeps it for the life of the
process, so the hold says nothing about what the user is doing. It never pauses
control, even while that app is focused. Mappings are read only for processes
that hold a watched node, in bounded chunks; a holder that cannot be classified is
treated as a possible game.

The compositor plugin reports the process of the active window on every
activation. A holder is **part of the focused application** when any of these
holds; a relation that needs an unreadable fact does not exist, and the
compositor's own session, cgroup and windows never relate anything:

| Rule | The holder and the focused window's process |
| --- | --- |
| Same process | are one process |
| Ancestry | are ancestor and descendant (at most 32 parent hops; the walk stops at pid 1, the compositor and other users' processes), which covers gamescope and launchers |
| Private session | share a session id that is not the compositor's; shell launches each get their own session and their children inherit it |
| Private cgroup | share a cgroup v2 path that is not a login `session-*.scope`, not the root and not the compositor's, such as a QindaLutris `systemd-run --user --scope` game |
| Wine prefix | are both Wine processes (image `.exe`, or the `wine`/`wine64`/`wine-preloader`/`wine64-preloader` loader) with the same `WINEPREFIX`, unset meaning `~/.wine`; this joins `winedevice.exe`, which holds the HID nodes, to the game's window |

Focusing the application of a holder that is not a web engine stops desktop
output immediately, before the application can receive leaked desktop input, and
reports `game`. A focus change when no such holder exists releases nothing, so a
held Meta-drag that activates another window keeps working.

Accepted limits: a browser, Electron, NW.js or CEF game that reads the pad through
the Gamepad API shares it with desktop control while focused, so turn the
profile's desktop control off for that controller in Settings; and background
remapper daemons are not recognized, because nothing they hold is the focused
application's input. The relation rules are heuristics over process facts; a
window whose process the compositor cannot report (an X11 client without XRes)
cannot own a game.

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
| `GetSnapshot() -> s` | Version-2 JSON: schemaVersion, ready, revision (decimal string), steam, error, controllers |
| `Apply(s id, s patch, t expectedRevision) -> s` | Result JSON: ok, reason, revision; ≤128 KiB atomic patch; exact revision |
| `Reset(s id, t expectedRevision) -> s` | Save validated defaults through the same mutation boundary |
| `Changed(t revision)` | Rediscover authoritative snapshot |

Snapshots are at most 4 MiB with at most 64 profiles and 32 connected devices.
Fields include id/name/family, connection/capability truth, current suppression
reason (plus an optional sanitized `consumer` name, at most 32 characters, while
the reason is `game`; additive, so `schemaVersion` stays 2), RGB capability/LED
status, effective page and config. Config has enabled,
pointerStick, pointerSpeed, deadzone, touchpad, gyro, gyroSpeed, activePage and
1–12 pages (id/name/color/layer/bindings). Each controller config is ≤128 KiB;
the atomic profile file is ≤2 MiB and schemaVersion 2. Version-1 files remain
readable; legacy bindings patches address the active page. Unknown fields,
invalid page references, duplicates, invalid actions/keys and out-of-range
values reject the whole edit. One ordinary page must remain. Partial or failed
loads cannot overwrite the existing file on discovery. Settings discards replies from an old
service generation and clears edit authority on owner loss.
Edits remain busy until a fresh post-reply snapshot arrives. A change signal
received during a snapshot queues another read, so suppression/reconnect changes
cannot strand the next edit on an old revision. Binding/page editors retain drafts on refusal and close only after confirming
readback. Interactive controls rebind to
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
The plugin passes the active window's pid to the runtime on the compositor
thread; the worker alone reads `/proc`. A focus change requests a fresh scan and
invalidates held input only when the last observation had a holder that is not a
web engine; a scan computed for another focus is kept if it names no such holder
and rescanned if it does
([ADR-0428](../adr/0428-pause-controller-desktop-input-only-for-the-focused-application.md)).
The SDL runtime borrows a sink that outlives it; each held
token must be released on suppression/disconnect/destruction. Voice1 dictation and command requests
use its exact unique bus owner and current revision, with no replay after an
owner replacement. [ADR-0347](../adr/0347-controller-desktop-integration.md)
records the dependency and priority decision;
[ADR-0426](../adr/0426-dualsense-desktop-layout-and-held-actions.md) records the
DualSense family, gyro persistence and held action contract.
[ADR-0427](../adr/0427-share-programmable-input-actions-and-controller-layers.md)
supersedes its fixed chord/fullscreen rules with shared combinations, pages,
contact gyro, held layers and reusable input macros.
[ADR-0428](../adr/0428-pause-controller-desktop-input-only-for-the-focused-application.md)
supersedes its rule that any other same-user open pauses a controller.
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
  edits gated by authority, readback, Steam status, owner loss, the `game`
  wording and the optional sanitized consumer name.
- `qindaqt.controller-runtime`: SDL virtual DualSense layout, both voice actions, trigger chords,
  gyro press/hold/release and persisted preference, family-correct reset,
  stick/touchpad/gyro motion, suppression releases, neutral resume, Steam startup,
  and the focus lifecycle over fake `/proc`, `/sys` and `/dev` trees: a background
  or passive web-engine holder never pauses, focusing a game releases at once and
  reports `game`, focusing away resumes after neutral controls, and focus changes
  without a game never release held input.
- `qindaqt.controller-observation`: a blocked scan leaves the event loop live,
  coalesced requests, invalidated-result rejection, normal scheduling and joined
  interrupted teardown; focus-change invalidation only with a non-web-engine
  holder, fencing of the scan computed for the old focus, and rescans of results
  that name a holder under a stale focus.
- `qindaqt.controller-focus-ownership`: fake `/proc` trees for background versus
  focused holders, each passive web-engine marker (including a marker across a read
  chunk and past the read bound), every relation rule with the compositor's
  session and cgroup exclusions, the ancestry bounds, Wine prefixes, unknown
  compositor identity and name sanitization.
- `qindaqt.controller-bus`: private out-of-process provider, change-during-read
  convergence, mutation completion after readback, and dictation/command start, release and cancellation.

The expanded checks add page migration/round trips, hostile combinations and
references, any-finger contact gyro, finger handoff, layer/chord ordering,
virtual RGB callbacks, macro validation/playback/failure cleanup, native shortcut
lease ownership and real QML profile/combination/text/macro journeys. Protocol
and integration verification do not require hardware ownership. Physical audio
quality is a separate observation; virtual tests do not claim a live utterance.

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

### October 20 DualSense layout acceptance

The requested layout and fresh voice capture recovery are integrated on main at
`d4c85a4b5`. The signed r39 package is installed on qinda-top; the running
compositor adopted `qindaqt_controllers_r39` without restarting the compositor
or shell. Exact-revision Controllers1 patches and readback verify all 19 bindings
on the connected DualSense, three remembered DualSense profiles and its family
defaults, preserving motion preferences and other families. The saved file is
owner-only (0600).

The r38 build reuses the authenticated r35 image and rebuilds Settings/Input and
the controller plugin; five Settings paths and that plugin change, while 2,688
base paths retain their contents and modes. r39 uses the authenticated r38 image
and replaces only the controller plugin, retaining 2,693 other paths. This is an
explicit laptop repair over its installed r35 desktop; the existing shared r32
delivery catalog is unchanged. Older plugins cannot read the new action IDs.

The focused build and all six controller CTest groups pass (43 QtTest results,
zero failures/skips). The real QML controller section's capability smoke and the
installed Controllers route construction probe pass. All 2,411 installed files
and symlinks match the signed package. No live voice utterance was recorded and
no physical button press was synthesized; SDL and private-bus fixtures provide
the action evidence. Final package: 87,296,000 bytes, SHA-256
`2edd3ee9a80a18b5c9a88b7101fc1d3132f09a47aa08eb3e6af39c19ff4f0f96`.

### October 20 programmable profiles and macros acceptance

Source `0c1e29fb3` implements ADR-0427 and is integrated on main. The final
Portage-installed package is r42. Its controller plugin remains r40; r41/r42
update only Settings Input QML. The original compositor and shell stay alive.
Controllers1 schema 2 and InputActions1 schema 1 report ready, and the connected
DualSense reports active desktop control, main page and successful LED writes.

Authoritative readback verifies the four-page preset on all five DualSense
profiles; five other profiles and saved speeds are preserved. Contact-only gyro,
right-stick pointing, Meta plus left click, held movement/editing, Create cycling
and Options Escape are configured. Both saved files are mode 0600.

Fourteen focused CTest groups pass (128 QtTest results, no failures or skips),
including the production shortcut-client plugin link, virtual SDL/LED/contact
fixtures, schema migration, held ownership, text/macro cancellation and real QML
editing/confirmation lifetimes. Six candidate application open/shutdown cycles
and both final installed Controllers/Macros probes report ready with no QML
warnings. A live InputActions1 wait-only macro verifies native assignment,
playback and cleanup without typing or clicking into applications. No physical
button presses, live voice utterances or browser-video playback were synthesized.

The signed r42 image retains 2,699 r40 base paths and changes four Input QML
paths. Every one of its 2,420 installed files/symlinks matches the image. The
source archive is immutable `645fb4dfc` over the installed Harmony Settings
baseline, with tracked PIC and confirmation-lifetime patches. The existing r32
shared catalog is unchanged. Artifact: 87,674,880 bytes; SHA-256
`2533aade6b725d94b50995a0519a68191a3dae524045ecd5de62b5bd4095e92c`.
