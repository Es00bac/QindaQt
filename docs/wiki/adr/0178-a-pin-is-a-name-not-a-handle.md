# ADR-0178: a pin is a name, not a handle

- **Status:** Accepted
- **Date:** 2026-09-16
- **Owners:** Platform (audio service)
- **Supersedes:** None (completes [ADR-0174](0174-meters-are-a-stream-not-a-snapshot.md)'s automatic binding)
- **Superseded by:** None

## Context

[ADR-0174](0174-meters-are-a-stream-not-a-snapshot.md) attached hardware
strips and buses to the graph automatically — default device first, then by
serial. That makes a console work out of the box and is wrong the moment a user
has an opinion: a second microphone should be strip 2, headphones should be A2.
The reference console lets every strip and bus choose its device.

`SetBusTarget` already existed and wrote a device **handle** into the bus. Two
things were wrong with it. A handle is a serial the daemon assigns fresh on
every restart, so the choice could not survive a reboot. And the automatic rule
ran on every publication and overwrote the handle a moment after it was set —
the operation succeeded and changed nothing.

## Decision

### What a pin is

`Strip::pinnedSource` and `Bus::pinnedTarget`: the chosen device's `node.name`
(schema 5). Empty means automatic. The name is the one identity that survives a
reboot ([ADR-0175](0175-virtual-strips-and-buses-are-nodes-the-console-owns.md)
put it on every `Device` for this reason), and it goes into the console's
document with everything else the user decided
([ADR-0176](0176-the-console-remembers-itself.md)).

### How it is set

`SetStripSource` (new, kind 18) and `SetBusTarget` take a device **handle** —
that is what a client holds — and the coordinator resolves it to a name against
the retained snapshot before the console model sees the request. A live handle
that is not a device of the right kind (an output offered to a strip) is
rejected as `stale-handle`. An **invalid** handle means "back to automatic" and
is admitted as such by both the client's preflight and the service; it is the
one case where a handle-targeted operation carries no handle.

A pin is not a binding. The model records the name; the coordinator rebinds
against the retained graph immediately and again on every publication.

### How binding honours it

Pinned names are claimed before anything automatic is assigned, so the
automatic rule never hands a hardware strip the microphone the user pinned to
another one. A pinned element binds to the device of that name when the graph
has it, and to **nothing** when it does not: falling back to automatic would
quietly route another microphone through the strip the user configured for
this one. Unpinned elements take the automatic rule as before.

### Surface

Each hardware strip and physical bus in the Settings console gets a device
picker: "Automatic", then every device of the right kind, with the pinned one
marked. The index is derived from the bound serial, never stored, so a device
appearing or disappearing cannot leave the picker on the wrong row.

## Consequences

- The user's device choices survive restarts and reboots, and survive the
  device being unplugged and returning.
- `SetBusTarget` now does what it always claimed to.
- A pinned-but-absent device is visible as an unbound strip with a pin, which
  is honest: the microphone is unplugged, not replaced.

## Revisit when

- The picker should offer devices that are not currently present (the pinned
  one when unplugged); that needs the store to remember device labels, not
  only names.
- Per-channel trim and input pairing (two mono devices as one stereo strip)
  land; a pin then names a pair.
