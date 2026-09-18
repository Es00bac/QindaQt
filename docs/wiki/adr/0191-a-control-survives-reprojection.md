# ADR-0191: A control survives reprojection and owns its value

- **Status:** Accepted
- **Date:** 2026-09-17
- **Owners:** Audio applet; the same law is to be applied to the Settings audio
  route and the volume keys
- **Superseded by:** None

## Context

Dragging the volume slider in the audio applet moved it one step and then
stopped. Five separate rules produced that, and the first one found was not the
one that mattered most:

1. `AudioAppletController::beginRequest` refused a second request for an object
   while one was in flight, with the feedback "A change for this item is
   already in progress."
2. The row's `adjustable` predicate included `!root.pending`, so the slider
   **disabled itself** the moment its own first request was dispatched.
3. `stepSize` was `0.05`, so the one step that did land was 5 %.
4. `AudioApplet.qml` handed each `Repeater` the row *list*
   (`model: controller.deviceRows`). A `Repeater` given a `QVariantList`
   regenerates every delegate whenever that list is reassigned, and the
   controller reassigns it on every reprojection — including the one its own
   dispatch triggers. **The control the user was holding was destroyed and
   rebuilt on the first move**, which loses the pointer grab, the keyboard
   focus and any local state.
5. Nothing held the value the user asked for. The authoritative level rebound
   the instant the control was no longer pressed, so the handle jumped back to
   the level the service still reported and stayed there for the whole round
   trip. For a keyboard step — where Qt reports `pressed` only during the key
   press — this meant every arrow key asked for the same value again instead of
   advancing.

A comment in both row files described (2) as deliberate — "the slider disables
itself while the row is pending, so a pointer drag cannot spam concurrent
requests" — which is a real concern answered in the worst possible way: by
making the control unusable. But (4) alone is sufficient to break a drag, and
fixing (1), (2) and (3) without it leaves the drag broken.

(4) was found by accident and only because a test held a raw `QQuickItem *`
across a dispatch and dereferenced it afterwards: the test segfaulted on a
dangling item. `tst_audio_applet_qml.cpp` now keeps a `QPointer` across the
dispatch and asserts both that the item survives and that it is still the
window's mouse grabber.

## Decision

**A control survives reprojection.** Each `Repeater` is given the row *count*
and each delegate binds its row by index
(`row: controller.deviceRows[index]`). The count changes only when rows are
added or removed, so a reprojection updates values in place instead of
rebuilding the list, and the item under the finger stays alive. This applies to
the device rows, the stream rows and the console strips alike.

**A pressed control owns its value.** The authoritative level rebinds through a
`Binding` with `when: !pressed`, so a snapshot arriving mid-drag cannot move
the handle under the finger; on release the binding resumes and the next
snapshot is authoritative again. A plain `value:` binding cannot do this,
because Qt breaks it permanently the first time the control assigns to `value`
itself during a drag. The percent readout follows the control's own value, not
the snapshot, so it tracks the finger.

**An outstanding intent outranks the snapshot.** The value a control asked for
is carried in the row projection (`DeviceRow`/`StreamRow`, with
`volumeIsRequested`/`mutedIsRequested` naming its provenance) and the control
shows it until the service answers for it. The intent is released once the
object has no request in flight or queued **and** either a snapshot newer than
the one the request was made against has arrived, or the request resolved with
any status other than `Succeeded`. The first clause keeps a service that has
not published yet from yanking the handle; the second is there because a
refusal usually changes nothing, so no newer revision may ever arrive, and
waiting for one would park the handle on a value the service has already
refused. Keeping this in the projection rather than in
QML is what makes it survivable and testable: QML holds no state that a
reprojection could lose.

**Pending is never a gate.** It remains a presentation state — muted labels,
accessible descriptions — and no control is disabled because its own request is
in flight. What a control *may* be disabled for is unchanged: a missing
capability, an unknown value, or a denied policy grant.

**Dispatch coalesces latest-wins, per object and per kind.** One request in
flight per object; at most one queued volume and one queued mute; a new value
replaces the queued one rather than being refused. When the in-flight request
completes the queue drains, so a drag sends the value the finger is on at that
moment — never a backlog of stale intermediate values, and never a refusal.
A queued mute dispatches before a queued volume, because a user reaching for
mute wants silence now.

**No fixed inter-request delay.** One-request-in-flight already limits the
dispatch rate to the service's own completion rate, which is the correct
backpressure: a fast service gets a smooth drag and a slow one is not flooded.
A fixed 30 ms floor would add latency without adding protection.

**Steps are fine-grained.** `stepSize` is `0.01`, so arrow keys move 1 % and a
drag resolves to 1 %. Wheel scrolling over a slider is enabled.

Queued intent is dropped with its serial. A value queued against an object that
leaves the graph must not dispatch later against whatever object reuses the
serial.

## Consequences

- The applet's volume sliders are draggable, and a drag is smooth rather than a
  single jump. Arrow keys move 1 % instead of 5 %, and successive arrow keys
  advance instead of re-asking for the same value.
- Binding a row by index means a change that does not alter the row count
  rebinds an existing item to a different object. **This is reachable in the
  overflow regime.** The device window is 8 rows across outputs and inputs
  together, so with nine or more devices an arrival or departure slides the
  window while the count stays at 8, and every row from the shift point on
  names a different device. The outstanding-intent map is keyed by serial, so
  no row ever *displays* another device's value, but a slider held across that
  moment dispatches its next move to whichever device now occupies the index.
  Below nine devices the count always changes with the membership and the
  window cannot slide, so the ordinary case is safe. Keying delegates by serial
  — which means a real `QAbstractListModel` rather than a `QVariantList` — is
  the fix, and it is deliberately not in this change: it is a larger rewrite
  than the defect being repaired here, and the overflow window is itself due a
  rethink. Recorded rather than hidden.
- `secondRequestWhilePendingIsRefused` is gone as a contract. Its replacement
  asserts the opposite: four moves during one in-flight request all return
  true, none dispatch, none produce feedback, and the completion dispatches the
  last value only.
- A snapshot can no longer fight the user, but it also cannot correct a pressed
  control. A control held down while the service rejects the change shows the
  user's value until release; the rejection is in the feedback line and the
  authoritative value returns on release. That is the right trade for a
  continuous control and the wrong one for a discrete one, which is why mute is
  a switch that commits immediately.
- The queue is bounded at one per object per kind, so no amount of dragging can
  grow memory or produce a burst on completion.
- A queued value whose dispatch is later refused is *not* rolled back at the
  point of refusal, unlike a refusal on the direct path. It does not need to
  be: every way a queued dispatch can fail — the object losing the capability,
  or leaving the graph — arrives in a newer snapshot revision or a new epoch,
  which makes the intent "settled", and the `reproject()` that immediately
  follows the drain releases it in the same turn. That holds only because a
  control whose capability is gone disables itself and so cannot queue anything
  after the loss: the queued value is always older than the snapshot that
  refuses it, which is what makes the snapshot newer than the request. A rollback at the refusal
  site would be unreachable code, so there is none.
- The row projection is not read only by the applet. The desktop status lane
  (`src/shell/desktop_controls/.../system_status_controller.cpp`) reads
  `deviceRows()` for its volume icon and summary, so during a round trip it
  reports the value the user asked for rather than the service's. That is the
  same answer the handle and the applet icon give, which is why it is left
  alone, but it is a consumer of this decision and is named here so the next
  change to it knows.
- **Not in this change:** the Settings audio route's faders and channel strips
  still commit on release only, and its model still tracks one global pending
  intent that a later request silently supersedes. The volume keys have their
  own step constants. Both need the same law; this ADR is the law they will be
  held to.

## Decomposition

`audio_applet_controller.cpp` reaches 544 non-blank lines with this change and
so crosses the 500-line review threshold. It is not decomposed here, and the
reason is that the obvious seam is worth doing deliberately rather than under
this repair: the per-object request bookkeeping — the in-flight map, the two
queues, the outstanding-intent map, and the four rules that move values between
them — is a coherent unit with no Qt, no client and no QML in it, and it would
test far better as its own pure ledger than through the controller's public
surface. That extraction is the next change to this file, not this one. The
file's own warning is not a net regression: splitting the controller test suite
in this same change retired the identical warning on
`tst_audio_applet_controller.cpp`, so the tree's warning count is unchanged at
78.

## Revisit when

- The Audio1 service gains the ability to accept concurrent operations for one
  object, which would let a mute dispatch immediately instead of at the next
  completion.
- A continuous control appears whose service round trip is slow enough that a
  visible fixed spacing would help rather than hurt.
