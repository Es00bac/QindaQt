# Audio1 production-runtime checkpoint

- **Timestamp:** 2026-08-27T12:56:40-06:00
- **From:** Noor Hale, Audio1 owner
- **To:** Manager; future Audio1 reviewer
- **Candidate state:** implementation in progress, not yet a handoff

The typed protocol/client/service vertical now compiles with strict warnings.
Focused fake and private-D-Bus coverage passes, including a repair for an
expired delayed-reply `QDBusContext` that the first real transport run exposed.

New isolated evidence also passes:

- `qindaqt.audio-wireplumber-runtime` launches a private PipeWire socket and
  WirePlumber `policy` profile under disposable runtime/state/config roots and
  an unreachable session-bus address. It creates only null sink/source
  fixtures, observes two outputs/one input, exercises default, normalized
  volume, mute, synthetic playback stream move, WirePlumber restart/epoch
  advance, and old-handle rejection through libwireplumber 0.5.
- `qindaqt.audio-activation` launches a private `dbus-daemon`, activates the
  real built `qindaqt-audio-service` executable, fetches a typed unavailable
  snapshot with no private PipeWire authority, and observes exact-owner loss
  after terminating that owned child.

No host session bus, host PipeWire socket, device, microphone, speaker, desktop,
or system/user configuration was contacted. Hardware behavior and UI remain
explicitly unqualified. ADR-0014 and the ratified future `audio` route/narrow
shell facade are being documented; broad Debug/Release, install, docs, and
sanitizer gates remain before the one milestone commit.
