# ADR-0347: Controller desktop input yields to games

- Status: Accepted
- Date: 2026-10-03
- Owners: Controller platform and Settings Input modules
- Scope: Compositor input, controller preferences and Controllers1

## Context

Controller desktop input must support Xbox, PlayStation, Nintendo and other
mapped devices without stealing their buttons from games. The existing KWin
evdev plugin has fixed bindings, no Settings boundary, and cannot observe a
consumer that opened its device before the plugin started. Touchpad and gyro
support require controller-specific reporting. Rebuilding the whole compositor
for controller policy is unnecessary because QindaQt already consumes its
versioned plugin SDK.

## Decision

An independent QindaQt compositor plugin uses SDL3's gamepad mapping, touchpad
and sensor APIs. It never grabs a device. The controller module separates
validated profile values, atomic user persistence, Linux consumer observation,
SDL sampling, compositor input delivery, and Voice1 hold capture.

Steam has unconditional priority while any same-user Steam process is running.
The plugin closes its controller handles and quits its owned SDL gamepad
subsystem reference. Other same-user opens of related input/js/hidraw nodes,
fullscreen apps, locked screens and inactive sessions suppress desktop output.
Initial process inspection covers pre-existing opens; inotify updates that
inspection on device access, with a one-second fallback. Suppression and
disconnect release held keys/buttons and cancel controller-owned dictation.
Resumption waits for all controls to return to neutral. Sony touchpads' duplicate
libinput desktop route is disabled while the plugin is loaded, then restored.

`org.qindaqt.Controllers1` exports bounded version-1 JSON snapshots and patches
on the session bus. KWin owns this name and `/org/qindaqt/Controllers1`; Settings
uses an asynchronous public port. Edits require the exact current revision,
are validated as one atomic patch, and are acknowledged only after saving.
Bindings name enumerated desktop actions or one keyboard chord; executable
commands are outside the contract. Per-device profiles derive from SDL identity
and serial. Identical devices with no serial share preferences, but their held
input tokens remain separate. Four family defaults are editable without hardware.

Controller dictation calls the exact current Voice1 owner with fresh revisions.
It starts on press and finishes on release; loss of desktop ownership cancels it.
The controller does not reserve the keyboard's Insert key. DualSense microphone
audio remains in Audio1/PipeWire: a USB-connected microphone can be selected as
the system default input used by Voice1. Bluetooth audio is not promised.

## Consequences

The controller plugin can be built and tested independently against the installed
compositor SDK, without compiling KWin. Settings adds the Controllers destination
to Input and search. SDL virtual fixtures qualify button, stick, touchpad, gyro,
Steam priority and neutral-resume behavior; physical controller and USB microphone
checks remain explicit hardware acceptance evidence.

See [controller desktop input](../architecture/controller-input.md) and
[Input settings](../apps/input-settings.md).
