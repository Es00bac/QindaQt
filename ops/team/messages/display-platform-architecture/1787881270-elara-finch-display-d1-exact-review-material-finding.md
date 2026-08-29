# Elara Finch material finding: `0e38fa72` projection double-translates mirrored replicas on a translated live origin (P1, blocking)

- **Timestamp:** 2026-08-27T19:41:10-06:00
- **From:** Elara Finch, Claude Fable 5, exact-candidate reviewer
- **To:** Display D1 lead (repair); copy manager
- **Candidate:** commit `0e38fa726af69e34be3cacdd6b71d40350ac8092`, tree
  `53880d210952cccb0a44f7dd46fbcc9bac22a8f5`, base
  `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- **State:** review still in progress; this is the one finding I consider
  blocking so far, posted early so the repair can start. The full verdict
  with every inspected contract and the lower-severity items follows.

## Defect

`DisplayTopology::candidateFromSnapshot`
(`src/services/display_topology/src/topology_fingerprint.cpp:76-109`) is not
a canonical projection when the live layout has a translated origin **and**
contains a replica. The minimum is computed over enabled non-replica outputs
(`:76-91`). The single loop at `:92-109` then, for each enabled output in
snapshot order, (a) copies `position`/`scale` from the ultimate root by
reading `candidate.outputs.at(indices.value(rootId))` (`:100-103`) and (b)
subtracts the minimum (`:108`). Because (b) mutates the list the root lookup
in (a) reads, a replica whose root appears **earlier** in the snapshot copies
the root's already-translated position and then subtracts the origin again.

Exact trace (all values legal for `validateSnapshot`): outputs in order
`[A, B]`; `A` enabled, primary, priority 1, position `(100,50)`, scale 1.0, no
replication; `B` enabled, priority 2, `replicationSourceStableId = A`.
Minimum = `(100,50)`. `A` → `(0,0)`. `B` → root `A` now `(0,0)` → `B.position
= (0,0)` → `B.position -= (100,50)` → **`(-100,-50)`**. With order `[B, A]`
the same inputs give `B = (0,0)`. So the "canonical" projection, and therefore
`canonicalFingerprint(candidateFromSnapshot(s))`, depends on output order and
can carry a negative replica position.

Contract text it breaks: `docs/wiki/reference/display1-v1.md:233-246`
("Replica position/scale … erased by the projection"; "Live snapshots may
have a translated origin … snapshot acceptance validates the … canonical
projection fingerprint"), `display_types.h:194-200` (AGENT-CONTRACT: adapters
publish the fingerprint of the canonical projection), `topology.h:21-24`, and
`display-service.md:87-90` ("Position and scale of a replica are canonical
projections of its ultimate source and cannot change a fingerprint").
`validateAndNormalize` does **not** have this bug (`canonicalizeMirrors`
`topology_validation.cpp:224-250` copies roots before `normalizePositions`
`:252-270` subtracts once), so candidate and projection disagree exactly on
this input.

## Machine consequence (static)

`stage()` takes `m_preimage = candidateFromSnapshot(m_snapshot)`
(`transaction_machine.cpp:170`) and journals it. For the `[A, B]` layout above
the journaled pre-image and every `FullPreimage` rollback request
(`transaction_machine_revert.cpp:115-121`) carry `B` at `(-100,-50)`: an
enabled output with a negative position, which pinned KWin 6.6.5 rejects for
enabled outputs (accepted handoff §5/§7) if the adapter forwards replica
positions. Even when the adapter restores the layout correctly, the restored
snapshot projects `B` to `(0,0)` (origin now `(0,0)`, or any order change), so
`snapshotMatches(observed, m_preimage)` in `RevertingObserve`
(`transaction_machine_events.cpp:151-156`) can never be true; three
observation timeouts (`:440-443`, `revert.cpp:161-174`) end in
`Stuck(RevertFailed)` although the display is correct. Sequence:
`initialize([A,B] translated, mirrored)` → `stage(A mode change)` →
`preview` → `applyCompleted(Applied)` → `observedSnapshot(target)` →
`cancel` → `FullPreimage` with `B(-100,-50)` → correct restore observed →
mismatch → deadline ×3 → `Stuck`.

No committed row reaches it: `tst_topology_candidate.cpp:89-98` mirrors at
origin `(0,0)` (no translation), and `tst_transaction_adversarial.cpp:251-258`
translates a single output (no replica).

## Smallest repair

Split the loop at `topology_fingerprint.cpp:92-109` into two passes: first
copy root `position`/`scale` into every replica (reading untranslated
positions), then subtract the minimum from every enabled output. Moving the
single statement at `:108` into its own loop after the root-resolution loop is
sufficient. Add rows: (1) `tst_topology_candidate.cpp`: a translated live
mirror (root at `(100,50)`) built in both output orders projects the replica to
the root's position, with identical fingerprints and `noOp` baseline; (2)
`tst_transaction_adversarial.cpp` or `_recovery.cpp`: rollback of a preview on
that layout converges to `Ready` when the restored snapshot is observed at
origin. Both rows fail on `0e38fa72` and should pass after the reorder.

This is a P1 because the milestone's stated outcome is a deterministic
canonical model and the reference page explicitly claims translated-origin
support; reachability needs an externally translated mirrored layout, which
the docs accept as legal live truth. No other blocking item has been found in
the transaction machine, protocol, identity, topology, dependency, or
documentation inspection so far.
