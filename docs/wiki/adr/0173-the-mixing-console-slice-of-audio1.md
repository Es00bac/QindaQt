# ADR-0173: the mixing-console slice of Audio1

- **Status:** Accepted
- **Date:** 2026-09-16
- **Owners:** Platform (audio service)
- **Supersedes:** None (implements [ADR-0123](0123-voicemeeter-class-audio-graph-on-pipewire.md) slice S4)
- **Superseded by:** None

## Context

ADR-0123 committed QindaQt to a VoiceMeeter-class console over PipeWire and
shipped slice S1 only: per-channel volumes, channel maps, managed virtual
devices. Everything that makes the reference product a *console* was missing,
and the full target is written down in
[VoiceMeeter Potato parity](../reference/voicemeeter-potato-parity.md).

Devices and streams describe **what exists**. A console describes **how the user
has wired it**, and the two are not the same model: a console has input strips
that each feed *several* output buses simultaneously, with an independent gain
on every strip-to-bus link. No arrangement of device and stream volumes
expresses that.

## Decision

Audio1 grows a versioned console slice (schema 3).

### Model

`Strip` (hardware or virtual input), `Bus` (physical or virtual output),
`MatrixSend` (one matrix cell: destination bus, on/off, and its **own gain**),
and `Level` (a dBFS meter reading). `Console` carries the strips, the buses, and
a published `soloActive`.

Identity is stable and human-meaningful — `strip.hw.1`, `bus.a1` — never a graph
id, so a user's routing survives a device disappearing, the service restarting,
and a reboot. Every level is decibels through the single gain law of
[ADR-0171](0171-one-gain-law-for-the-audio-console.md); nothing carries a linear
scalar.

The default layout is the reference console's shape: 5 hardware and 3 virtual
input strips against 5 physical (A1–A5) and 3 virtual (B1–B3) output buses. The
matrix is **rectangular** — every strip carries a cell for every bus — so a
client can index it without first asking which cells exist.

### Ownership

`ConsoleModel` is pure: no PipeWire, no D-Bus, no filesystem. It answers what
the console looks like and what routing it implies; the backend builds that
routing. That split is what makes a console testable without an audio daemon.

Console operations are applied **in the coordinator and never submitted to the
graph backend**. A console change therefore completes synchronously rather than
waiting on a graph round trip, which is what makes a fader feel attached to the
sound. The graph backend fails loudly if one reaches it, rather than dropping it.

Two consequences follow and are both guarded:

- Every backend snapshot has the console folded back in. Without that, a device
  appearing or disappearing would blank the user's whole console on the next
  publication.
- A malformed backend payload degrades availability but **keeps** the console,
  because it is the user's configuration rather than a projection of the graph.

### Routing

`ConsoleModel::routing()` yields one edge per enabled send, each carrying its
gain and an `audible` flag. Mute, solo and bus mute set `audible` false; they do
**not** remove the edge. A console that tore its graph down on solo would take
audible time to come back and would lose the mix on unsolo.

A disabled send keeps its gain, so turning a send off and on again restores the
level the user dialled in instead of slamming it to unity.

### Admission

`validateConsole` is fail-closed: bounded ids and labels, no duplicate strip or
bus identity, no send naming a bus the console does not publish, gains inside
the published fader range, meters inside dBFS, and `soloActive` consistent with
the strips. Meter values and fader values are validated against **different**
scales on purpose; interchanging them is a real and easy mistake.

## Consequences

- QindaQt has a real routing matrix with per-send gain, which is the feature the
  device/stream model could not express at all.
- The console is configuration, not a view of the graph: it persists, and it
  survives every device behind it going away.
- Clients that do not know the console capability bits ignore the slice and keep
  working against devices and streams.
- The wire signature of `GetSnapshot` changed, so every in-repo consumer moved
  in the same change, as ADR-0123 requires of a versioned additive slice.

## Revisit when

- The backend realises `routing()` as PipeWire loopbacks with per-link volume;
  the edge list is already shaped for a diff against the live graph.
- Metering produces real readings; `Level` is on the wire and validated, and
  nothing publishes into it yet.
- Per-strip DSP lands (gate, denoiser, compressor, limiter, EQ). Each is a
  `filter-chain` node in the strip's path and an additive block on `Strip`.
