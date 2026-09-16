# ADR-0174: meters are a stream, not a snapshot

- **Status:** Accepted
- **Date:** 2026-09-16
- **Owners:** Platform (audio service)
- **Supersedes:** None (completes a gap named in [ADR-0173](0173-the-mixing-console-slice-of-audio1.md))
- **Superseded by:** None

## Context

[ADR-0173](0173-the-mixing-console-slice-of-audio1.md) put `Level` on the wire,
validated it, and drew it — and left its own note that *nothing publishes
readings yet*. A console whose meters never move is not a console; a user
setting a gain stage has nothing to set it against.

Two problems stood between the drawn meter and a real one.

**Nothing measured.** PipeWire exposes no "level of this node" property and
WirePlumber wraps none. A meter is not a query; it is an audio consumer.

**Nothing to measure.** The console's strips and buses were never attached to
any device. `bindStripSource` and `bindBusTarget` existed and no code called
them, so no routing edge was ever buildable and no meter would have had a node
to read even if one had asked.

## Decision

### Measuring

One `pw_stream` per metered node, capturing into a fixed F32 format that
PipeWire's adapter converts for us. Its `on_process` runs on PipeWire's
**realtime data thread** and does exactly one thing: walk the buffer, track the
peak, accumulate energy. The only state crossing back to the service is three
lock-free atomics per stream. No allocation, no lock, no Qt object touches that
thread — a realtime violation here is heard as a dropout, not seen as a crash.

Peak is a **hold** until the consumer reads it, so a transient between two polls
still lights the meter. Reading clears it.

A meter stream is named `qindaqt.meter.<console id>` — deliberately **not** the
`qindaqt.virtual.` prefix, so code that enumerates virtual devices by name never
finds a meter — and is marked passive and virtual so it cannot keep a device
awake or appear as somewhere the user could route audio.

### Attaching

The coordinator follows the graph with the console's endpoints on every accepted
snapshot: hardware strips claim capture devices and physical buses claim output
devices, the **default** device first because that is what the user means by
"my microphone", then the rest by serial so the assignment is stable. Managed
virtual devices are excluded: they are the console's own endpoints, not hardware
the user plugged in.

An endpoint past the end of the available devices stays **unbound** rather than
sharing one device with another strip — two faders on one microphone would look
like a console that works and behave like one that does not.

### Publishing

Meters do **not** travel in snapshots. `LevelReading` is a separate value on a
separate `Levels` signal, carrying no lineage.

This is the whole point. A snapshot is the console's *configuration*, and its
revision is what clients diff on. Republishing it twenty times a second would
rebuild and revalidate the entire console at meter rate, would make every
revision number meaningless, and would have each client refetch the full
snapshot for a number that is about to change again. Readings are still folded
into the retained snapshot, so a client connecting mid-stream sees live meters
in its first `GetSnapshot` — but folding them in advances nothing.

An unchanged batch is not sent. A silent console therefore costs nothing on the
bus, because a silent capture stream produces the identical reading every poll.
An **empty** batch means metering stopped, and every meter returns to unknown
rather than freezing on its last value.

The Settings route follows the same rule: `consoleLevels` is a property with its
own notification, never part of `viewChanged`, so a level frame never rebuilds a
strip delegate.

### Two defects this exposed

Attaching the console to real audio made two latent faults visible immediately,
and both are fixed here because a meter cannot be verified around them.

**Every console operation was rejected as a stale handle.** Admission treated
"not `CreateVirtualDevice`" as "names a device handle", so a request carrying a
console id and no handle at all failed its lineage check before it ever reached
the console model. The entire console D-Bus surface had never worked. There is
now one shared `operationTargetsHandle` used by both the client's preflight and
the service's admission, so the two cannot drift; `SetBusTarget` is the single
console kind that does carry a device.

**A send loopback attached to the wrong node and then refused to play.** It
addressed its endpoints with the deprecated `node.target`, which wants an object
id: given a name it resolves nothing and silently autoconnects the *default*
device instead — so the matrix looked wired and was not. Both sides now use
`target.object`, which is the only key that takes a name. The same argument
marked both sides `node.passive`, and a passive link will not resume a suspended
device, so an enabled cell produced silence; an enabled cell is the user saying
"carry this audio", and neither side is passive any more. `stream.capture.sink`
was also hard-coded true, which is right for a virtual strip's null sink and
wrong for every hardware input, so it now follows the strip's kind.

## Consequences

- The console's meters move, which is what makes a gain stage settable at all.
- The console is attached to real hardware out of the box, with no configuration
  step: the same binding that feeds the meters is what makes a routing edge
  buildable.
- Metering costs one capture stream per bound endpoint. They are passive and
  hidden, and stop with the console.
- A malformed reading — an RMS above its own peak, a level above full scale —
  is refused at the service and again at the client, so it cannot move a meter
  at either end.
- A strip meter is passive: it never resumes a suspended device, so QindaQt does
  not switch on a microphone or light a camera's recording indicator by itself.
  A strip lights when its audio is actually being carried — the user enabled a
  send, or another application already has the input open. Bus meters read an
  output's monitor, which turns nothing on, and are live immediately.

## Revisit when

- Virtual strips and buses get their managed sinks; they are metered by the same
  path the moment they are bound, using the sink-monitor form the target
  declaration already carries.
- The user can pin a strip's source explicitly; automatic binding then applies
  only to endpoints the user has not chosen.
- Meters gain a subscription (`AcquireMeters`/`ReleaseMeters`, refcounted per
  caller and released on name loss). That is what will let an idle console show
  input levels while it is on screen without holding those inputs open for as
  long as the desktop runs — the consent boundary, made explicit and temporary
  instead of permanent and passive.
- Per-strip DSP lands and the meter needs a defined tap point — before the
  fader, after the processors, or both.
