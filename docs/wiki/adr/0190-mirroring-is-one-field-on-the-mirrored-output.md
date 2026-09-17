# ADR-0190: Mirroring is one field on the mirrored output

- **Status:** Accepted
- **Date:** 2026-09-17
- **Owners:** Display Settings route and the Display1 client
- **Superseded by:** None

## Context

Display1 has modelled mirroring since the wire was defined: every output
carries `replicationSourceStableId`, the writer port applies it, and
`validateSnapshot` rejects a source that names no existing output or the output
itself. Nothing in QindaQt ever wrote it. There was no mirror control anywhere,
so two displays could only ever extend.

Rotation had a different problem. The Display route's Orientation section works
— on `qinda-top` Display1 is `Ready`, the output reports `enabled`, and
`canEdit` is true — but it is the fifth section on a long page, and the user
reported there is "no rotation control". Nothing above it said the display was
rotated either, so a rotated display looked like a broken one.

## Decision

**Mirroring is one operation on the mirrored output.**
`DisplaySettingsModel::setOutputMirror(stableId, sourceStableId)` sets
`replicationSourceStableId` in the draft, and an empty source clears it. The
service validates only that the source names another existing output, so the
rest is this model's policy and it refuses without touching the draft:

- an output may not mirror itself;
- the source must exist and be enabled, because a disabled output has no
  pixels to copy;
- the source may not itself be mirroring — no chains, so anything resolving a
  replication source transitively cannot find an ambiguous answer;
- a mirrored output is enabled, and takes the source's position, because it
  shows the same pixels in the same place. A mirrored output at its own
  coordinates would leave the arrangement diagram claiming two positions for
  one image.

Choosing **Extend** again restores the position the output had *before* it
started mirroring, kept in a draft-local field that never reaches the wire.
Falling back to the canonical "place it right of everything else" rule would
silently rearrange a desktop the user had already arranged.

**`DisplayMirrorRow`** is the only surface that writes the field. It offers
Extend plus one button per other enabled, not-itself-mirrored output — exactly
the set the model accepts, so no offered choice can be refused. With one
display it says so rather than showing a dead control. When the two logical
sizes differ it says which one is being scaled, because mirroring drives both
panels from one framebuffer and the result is visibly soft on the mismatched
one. The row sits in the Arrangement section: mirroring is the other answer to
"where do these displays sit", and the exact-position controls below it are
meaningless for a mirrored output.

**The output card states non-default orientation and mirroring.** That is the
fix for the rotation report: the control was reachable, its state was not
visible, and the card is the first thing on the page.

## Consequences

- Two displays can mirror. A configuration that was expressible on the wire and
  in the compositor for months is now reachable.
- A rotated or mirrored display is identifiable without scrolling, and the same
  text is in the card's accessible description.
- `OutputDraft` gains two draft-local fields (`positionBeforeMirror`,
  `hasPositionBeforeMirror`). They are not part of the Display1 wire, never
  reach a candidate, and are lost when a snapshot replaces the draft — after
  which Extend falls back to the canonical placement, which is the honest
  behaviour when the earlier position is genuinely unknown.
- The mirror row is a set of radio buttons rather than a combo box, so the
  current state is readable without opening anything. With many displays that
  row grows; a combo box becomes the better shape at four or more.
- **Not done here, and deliberately:** the arrangement diagram still draws a
  mirrored output as its own tile stacked exactly on its source rather than
  collapsing them into one. It is correct — they occupy the same rectangle —
  but it reads as one tile by accident rather than by design.
- **Refresh rate** needs no new control: the mode combo lists every
  server-advertised mode including its refresh rate, so every rate is already
  selectable. Splitting resolution and refresh into two pickers is an
  ergonomics improvement, not a missing capability.

## Revisit when

- A third display makes the radio-button row too wide.
- The arrangement diagram grows real mirrored-group rendering.
- KWin exposes VRR policy through Display1, which the wire models but no
  control reads.
