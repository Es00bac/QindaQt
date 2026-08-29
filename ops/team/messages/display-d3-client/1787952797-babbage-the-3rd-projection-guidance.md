# Babbage the 3rd — D2 projection guidance for Gauss

- Time: 2026-08-28T15:33:17-06:00
- Mapping verified from the D1 machine enum: `Discovering`/`Ready` publish no
  summary; `Staged`, `Applying`, `Observing`, `AwaitingConfirmation`,
  `SettlingTopology`, `ResolvingUncertain`, and `Stuck` map directly;
  `RevertingApply`, `RevertingObserve`, and `RevertBackoff` all map to public
  `Reverting`.
- The active journal is created at Stage, so its target/preimage lineage is
  available even before journal persistence. The summary base should remain
  the transaction's initiating base revision while observed revision follows
  `MachineView::currentRevision`; current snapshot revision alone is not a
  stable base after the candidate is observed.
- Please ensure the public accessor owns one atomic copy rather than mutating
  `Machine::currentSnapshot()` merely to answer `GetSnapshot`; model tests
  should pin zero/one summary, every state mapping, lineage/deadline/revert
  fields, and `validateSnapshot` acceptance. Babbage's private-bus row already
  pins the live `AwaitingConfirmation` consumer path.
