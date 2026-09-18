# ADR-0202: QindaQt provisions OBS and owns one secret

- **Status:** Accepted
- **Date:** 2026-09-17
- **Owners:** Streaming (Settings Streaming route, packaging)
- **Supersedes:** None
- **Superseded by:** None

## Context

OBS out of the box has no obs-websocket password, no profile suited to a
desktop, and no scene collection. A user who installs QindaQt and presses
"Start recording" in the top bar would otherwise meet a chain of failures
with no diagnosis — and "Start Virtual Camera" would fail outright, because
nothing installed the `v4l2loopback` configuration that gives OBS a device to
write into, even though those two files have shipped in this tree since the
streaming work began.

## Decision

**QindaQt writes exactly three things into OBS's configuration, under names
it owns, and stores exactly one secret.**

- One profile and one scene collection, both named `QindaQt`, plus the
  obs-websocket server settings. The key names are obs-websocket's own, read
  out of the installed plugin binary rather than guessed.
- **This is provisioning, not ownership.** QindaQt never edits a profile or
  collection the user made, never renames theirs, and never switches OBS to
  its own behind their back: it writes files, and OBS's UI decides what is
  current. Writes are atomic, so a crash cannot leave OBS a truncated profile
  it then refuses to load.
- The scene collection deliberately does **not** declare the console bus and
  strip sources. The OBS bridge creates and removes those to match Audio1
  (ADR-0190); a collection that also declared them would fight the bridge on
  every console change.
- The profile declares no streaming service and no stream key. QindaQt does
  not know the user's channel and must not invent one.
- **The obs-websocket password lives in the Secret Service**, under one set
  of attributes scoped to this one secret. There is no file fallback: a
  password written beside OBS's configuration in the clear would be a worse
  promise than "the keyring is not available". A keyring that cannot be read
  is reported as such and is never treated as "there is no password", because
  the second would overwrite a credential OBS already has.
- **The package installs the virtual-camera module configuration.**
  `modprobe.d` and `modules-load.d` files go to `lib/…` under the prefix, not
  `lib64`, because that is where kmod and systemd-modules-load look.

## Consequences

- `media-video/obs-studio[screencast,v4l,wayland,websocket]` and
  `media-video/v4l2loopback` become runtime dependencies of
  `gui-wm/qindaqt-desktop`.
- Setting OBS up is an explicit button in Settings → Streaming, not something
  that happens silently at login. It reports what it did, including that OBS
  must be restarted to read the new configuration.
- Three Settings1 keys (`services.obsWebSocketPort`, `services.obsAutoConnect`,
  `services.obsStartAtLogin`) are new; Settings1 rejects a whole snapshot on
  one unknown key (ADR-0126), so the resident settings service must know them
  before the route can save anything.
- QindaQt's `first_load` setting suppresses obs-websocket's own welcome
  dialog, which would otherwise generate a second password that is not the
  one in the keyring.

## Revisit when

- OBS gains a supported way for a desktop to register a control client
  without writing its configuration files.
- A second QindaQt surface needs a stored credential, which would justify a
  general secrets module rather than this scoped store.
