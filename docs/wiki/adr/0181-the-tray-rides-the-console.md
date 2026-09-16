# ADR-0181: the tray rides the console

- **Status:** Accepted
- **Date:** 2026-09-16
- **Owners:** Platform (shell audio applet)
- **Supersedes:** None (extends the applet under [ADR-0173](0173-the-mixing-console-slice-of-audio1.md))
- **Superseded by:** None

## Context

The tray's audio applet showed devices and streams — the S1 slice — and nothing
of the console. A streamer who needs to pull a microphone down mid-session
should not have to open Settings and find the rack; the reference console's
tray icon opens its strips.

## Decision

The applet projects the console's strips as a third section, after devices and
streams: label, a horizontal fader, mute, and a meter. `ConsoleRow` is a value
gadget projected by the same pure `AudioAppletModel::project` as device and
stream rows, within a budget of eight strips so the tray stays a tray; an
unbound strip still shows, dimmed, so a mute set in Settings reads here too.

The fader is driven by **position** through the service's gain law, never a
linear mapping done in the applet, and commits once on release rather than
once per pixel. Both intents are gated by the same control grant as every
other intent, and carry no pending state: console operations complete at the
service synchronously.

Meters ride `consoleLevels`, a property with its own notification bound to the
client's level channel ([ADR-0174](0174-meters-are-a-stream-not-a-snapshot.md)),
never the reprojection signal — a level frame must not rebuild the applet.

## Consequences

- A strip's level can be ridden and muted from the tray.
- The applet's compiled QML module gains one file; the module's own tests
  cover the section through the stub controller.

## Revisit when

- The tray should offer the buses too; the same row shape fits with the bus's
  fader and mute.
