# ADR-0175: virtual strips and buses are nodes the console owns

- **Status:** Accepted
- **Date:** 2026-09-16
- **Owners:** Platform (audio service)
- **Supersedes:** None (completes [ADR-0173](0173-the-mixing-console-slice-of-audio1.md); builds on [ADR-0174](0174-meters-are-a-stream-not-a-snapshot.md))
- **Superseded by:** None

## Context

The reference console's whole value is its *virtual* channels: an application
plays into "Voicemeeter Input" and gets its own fader; a streamer's software
records "Voicemeeter Out B1" and gets a submix. [ADR-0173](0173-the-mixing-console-slice-of-audio1.md)
modelled those strips and buses and [ADR-0174](0174-meters-are-a-stream-not-a-snapshot.md)
attached the *hardware* ones to the graph, but a virtual strip or bus had no
node behind it at all — it drew, and carried nothing.

## Decision

### What a virtual endpoint is made of

A **virtual strip** is one null sink, `qindaqt.console.<strip id>`. Applications
choose it as their output; the console reads its monitor for the strip's meter
and for every send out of it.

A **virtual bus** is one `module-loopback` whose capture half is a sink,
`qindaqt.console.<bus id>`, and whose playback half is a real source,
`qindaqt.console.<bus id>.source`. The strips' sends play into the sink;
applications record from the source. It has to be two typed nodes: WirePlumber
will not link a playback stream into an `Audio/Source/Virtual` node even when
asked by name, and browsers hide monitor sources from their microphone pickers,
so a bus that was only a sink's monitor would be invisible to exactly the
applications a streamer needs it in.

Both halves of the bus, and the strip sink, carry `node.autoconnect = false`.
They are devices, not streams; left autoconnecting, WirePlumber treated a bus's
sink half as a stream wanting a target and linked its monitor into whatever
sink it picked — two virtual buses were found silently feeding a virtual strip.

### Who creates them, and how

The service does, on every accepted snapshot, for every declared endpoint the
graph does not already have — from a previous run of the service, or from
daemon configuration. The declaration is made the moment the backend starts,
not on the first snapshot: the endpoints are what the console is made of, and a
restarted backend rebuilds them from that declaration.

A bus is loaded as a loopback module in the service's own PipeWire context and
dies with it, like a send. A strip's sink is created **server-side** through the
raw `pw_core_create_object` API with `object.linger`, so it outlives both the
proxy and the service: an application playing into its strip keeps that output
across an audio-service restart instead of landing on the speakers.

**Not** `wp_node_new_from_factory`. That call hands WirePlumber a `WpNode` for
the new global, and an object manager on the same core never presents a global
whose proxy the core already owns. Across forty-two rebuilds the sink was in
the daemon, every other client could see it, and this service alone could not
find it to bind. A raw `pw_proxy` is not a `WpProxy`; the global is presented
like any foreign one. (The S1 `CreateVirtualDevice` path uses the WirePlumber
call and is likely subject to the same blindness within the session that
creates the device; it is out of scope here and noted below.)

The record of having asked, not the graph, is what says an endpoint is on its
way: a new node takes several rebuilds to be announced, and asking the graph in
that window created a copy per rebuild — a probe found twenty-two identical,
lingering "Virtual Input" sinks.

### Binding by name

Every `Device` now carries its `node.name` (schema 4). A serial is assigned by
the daemon and changes on every restart; the name is the one identity that
survives a reboot, and it is what the console binds its virtual endpoints to.
Hardware binding is unchanged, except that it now skips console-owned nodes as
it already skipped user-managed virtual devices: neither is hardware the user
plugged in.

### Monitor capture is a target

Every console meter, and every send out of a virtual strip, is a capture
stream reading a sink's monitor. The graph projection and the protocol's
validator both judged a capture stream's target only against inputs, so each
of those streams read as "target unknown" and **every snapshot the console
ever produced was Degraded (`graph-bounded`)**. A capture stream may now target
an output device; a playback stream into an input remains impossible.

## Consequences

- An application can be given its own fader by choosing "Virtual Input" as its
  output, and a streamer's software can record a submix from "B1" — the two
  things the reference console exists for.
- Virtual endpoints are metered by the same path as hardware ones the moment
  they are bound, in the sink-monitor form the meter declaration already
  carried.
- The console's snapshots are `Ready` again.
- Strip sinks persist in the daemon after the service exits. That is the point,
  and it means a design change that renames them must also remove the old ones.

## Revisit when

- The S1 `CreateVirtualDevice` path is moved onto the raw core API so a device
  the user creates is visible to the session that created it.
- The endpoints are also declared daemon-side in a shipped PipeWire drop-in, so
  they exist before the service starts; `ConsoleEndpoints` already names them
  from one definition for that reason.
- The user can rename a virtual strip or bus; the description follows the
  console label, and the node must be re-described rather than re-created.
