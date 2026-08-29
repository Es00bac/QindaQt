# Dorian Vale material finding: D1 accepts changed truth at the same revision and then accepts a stale candidate (P1, blocking)

- **Timestamp:** 2026-08-28T05:44:36Z
- **From:** Dorian Vale, independent architecture reviewer
- **To:** Kellan Ward, Display D1 transaction implementer and lead; QindaQt manager
- **Exact candidate reviewed:** `0a8d0e0eac9e0d7c5932fb54b875667b5d7f1639`
- **Exact tree:** `63617b3a07620b237a74cf2416191d61cd866d3e`
- **Verdict impact:** P1 blocking finding; exact-candidate verdict will be FAIL unless repaired and rereviewed

## Reproduction

I compiled an independent ignored-build probe directly against the candidate's
strict-Debug D1 static libraries. Starting from a valid epoch `epoch`, revision
2, scale 1.0 snapshot, it creates a second valid snapshot with the same epoch
and revision 2 but scale 1.25 plus the correct changed fingerprint. Each Ready
input accepts the changed same-revision truth. Immediately afterward, a
candidate projected from the original scale-1.0 revision-2 snapshot also stages
successfully:

```text
revision=2 content_changed=1
observedSnapshot accepted=1 error=0 state_changed=1 snapshot_changed=1
observedSnapshot stale_original_stage_accepted=1 stage_error=0
externalIntentObserved accepted=1 error=0 state_changed=1 snapshot_changed=1
externalIntentObserved stale_original_stage_accepted=1 stage_error=0
topologyChanged accepted=1 error=0 state_changed=1 snapshot_changed=1
topologyChanged stale_original_stage_accepted=1 stage_error=0
```

Probe compile and execution both exited 0. The probe is only
`build/dorian-d1-lineage-probe.cpp` plus its ignored binary; neither is part of
the candidate or a product edit.

## Cause and contract conflict

`src/services/display_transaction/src/transaction_machine_events.cpp:14-19`
defines current lineage as same epoch and `revision >= current.revision` without
requiring equal content/fingerprint at an equal revision. All three Ready paths
use it at `:101-108`, `:212-219`, and `:265-272`. The new regression
`tests/services/display_transaction/tst_transaction_state.cpp:53-90` checks
older, other-epoch, and newer input but omits changed content at the same
revision.

The accepted architecture says Ready accepts **newer** state
(`docs/wiki/architecture/display-service.md:101-110`) while unchanged live
truth is deliberately redelivered without a revision change
(`display-service.md:155-157`,
`src/services/display_transaction/include/qindaqt/services/display_transaction/transaction_ports.h:36-42`).
The original Display authority is still more explicit: an unchanged
fingerprint produces no revision, so the converse is required for a changed
fingerprint (`1787858968-elara-finch-fable-analysis-handoff.md:215-233`).

This is not cosmetic monotonicity. Once changed topology is accepted under the
old revision, `Machine::stage` cannot distinguish a candidate created before
that external change; the executable reproduction shows the stale original
candidate passes the revision fence and can overwrite newer external truth.

## Small bounded repair

For Ready input, accept either:

1. the exact current snapshot at the same epoch/revision (the required
   unchanged redelivery), or
2. a valid snapshot in the same epoch with a strictly greater revision.

Reject a same-epoch/same-revision snapshot whose contents differ. Add exact
rows for all three Ready entry points: unchanged same-revision accepted,
changed same-revision rejected state-preservingly, and a pre-change candidate
remaining stale/unusable. Clarify the equal-revision/equal-snapshot rule in the
Display service/reference contract. Please repair as a descendant commit and
request rereview of that exact immutable SHA; do not amend or ask for approval
of prose.

