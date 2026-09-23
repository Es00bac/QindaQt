# ADR-0240: Encode the KWin overview edge with KConfig semantics

- **Status:** Accepted
- **Date:** 2026-09-22
- **Owners:** Session, compositor integration
- **Clarifies:** [ADR-0232](0232-the-gather-overview-replaces-the-upper-left-corner.md)

## Context

[ADR-0232](0232-the-gather-overview-replaces-the-upper-left-corner.md)
released KWin's upper-left corner for QindaQt Gather by seeding
`[Effect-overview] BorderActivate` as an empty `QStringList` through
`QSettings`. Qt wrote that value as `BorderActivate=@Invalid()`. KWin reads the
entry through KConfig as an integer list; on this machine that encoding became
`[0]`, `ElectricTop`, and KWin's overview opened from the whole top edge. A
`QSettings`-only test incorrectly saw an empty string list and passed.

## Decision

The session seeds a valid empty `QString` instead. It persists as
`BorderActivate=`, which KConfig reads as an empty integer list. The seed
still leaves any valid user assignment intact. Existing `@Invalid()` entries
from the earlier release are repaired on session startup because they are the
known malformed seed, not a meaningful edge choice. The repair recognizes
both a reloaded invalid value and Qt's same-process cached empty string list.

The session-defaults regression test reads the result through KConfig with
KWin's nonempty default (`[7]`). It also creates the old malformed value and
checks its migration to an empty integer list. QindaQt's pointer reservation
and Gather dispatch remain as decided in ADR-0232.

## Consequences

This repair takes effect on the next session startup. For an existing running
session, writing the empty KConfig value and reconfiguring KWin releases its
top-edge reservation immediately; the QindaQt upper-left reservation already
belongs to the compositor plugin.
