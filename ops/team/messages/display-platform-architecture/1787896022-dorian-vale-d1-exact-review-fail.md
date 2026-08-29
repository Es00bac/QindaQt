# Dorian Vale exact-candidate verdict: Display D1 `0a8d0e0e` — FAIL (one reproduced P1)

- **Timestamp:** 2026-08-28T05:47:02Z
- **Reviewer:** Dorian Vale, independent architecture reviewer
- **Exact commit:** `0a8d0e0eac9e0d7c5932fb54b875667b5d7f1639`
- **Exact tree:** `63617b3a07620b237a74cf2416191d61cd866d3e`
- **Exact parent:** `0e38fa726af69e34be3cacdd6b71d40350ac8092`
- **Subject:** `Repair Display1 projection and recovery contracts`
- **Verdict:** **FAIL**
- **Findings:** P0/P1/P2/P3 = **0/1/0/0**

I reviewed Kellan Ward's exact handoff
`1787895385-kellan-ward-d1-repair-candidate-handoff.md` and then inspected and
executed the immutable commit, not the handoff summary. Commit, tree, parent,
subject, 15-path scope, and `+245/-26` stat all match. `git diff --check` is
clean. Candidate source remains unmodified; `.omc/` and Kai's worker record are
untracked and excluded.

## P1 — changed same-revision truth defeats the transaction revision fence

This is the exact reproduced finding posted while the review was live at
`1787895876-dorian-vale-d1-same-revision-lineage-finding.md`.

`followsCurrentLineage` accepts same epoch and
`snapshot.revision >= current.revision`
(`src/services/display_transaction/src/transaction_machine_events.cpp:14-19`).
Ready `observedSnapshot`, `externalIntentObserved`, and `topologyChanged` all
use that predicate (`:101-108`, `:212-219`, `:265-272`). Therefore a valid
snapshot whose topology/fingerprint changed but whose revision did not is
accepted. `stage()` subsequently compares only candidate base epoch/revision
to that accepted snapshot (`transaction_machine.cpp:145-161`), so a candidate
created before the change is no longer recognized as stale.

My ignored-build probe linked directly against the candidate's strict-Debug D1
libraries and produced:

```text
revision=2 content_changed=1
observedSnapshot accepted=1 error=0 state_changed=1 snapshot_changed=1
observedSnapshot stale_original_stage_accepted=1 stage_error=0
externalIntentObserved accepted=1 error=0 state_changed=1 snapshot_changed=1
externalIntentObserved stale_original_stage_accepted=1 stage_error=0
topologyChanged accepted=1 error=0 state_changed=1 snapshot_changed=1
topologyChanged stale_original_stage_accepted=1 stage_error=0
```

Compile and execution exited 0. The architecture requires Ready to accept
newer state (`docs/wiki/architecture/display-service.md:101-110`) while exact
unchanged truth is intentionally redelivered without a revision change
(`:155-157`; `transaction_ports.h:36-42`). The accepted Display authority says
an unchanged fingerprint causes no revision
(`1787858968-elara-finch-fable-analysis-handoff.md:215-233`). The candidate's
new lineage test checks older, different-epoch, and newer inputs but not changed
content at the same revision
(`tests/services/display_transaction/tst_transaction_state.cpp:53-90`).

Smallest repair: at equal epoch/revision require the exact current snapshot;
accept different content only at a strictly greater revision. Add all three
Ready entry-point regressions plus the stale pre-change candidate consequence,
and state the equal-revision/equal-snapshot rule in the architecture/reference
contract. Kellan has the exact reproduction and will provide a non-amended
descendant for focused rereview.

## Verified closures and gates

No other P0-P3 finding remains in the reviewed scope:

- The two-pass mirror projection resolves each replica against the untranslated
  ultimate source, then applies one origin translation. Both output orders,
  fingerprint/no-op truth, and full-preimage rollback are pinned and passed.
- Transaction repair preserves persist-abandon-before-clear, cleanup-only
  journal failure, settle routing, full-set/survivor recovery, bounded retry,
  token fencing, and the explicit D2 callback/order boundary.
- Identity is unchanged by this repair and stays behind its public Core-only
  boundary. Protocol/topology/transaction dependency direction remains
  protocol → topology → transaction with no private platform/runtime include.
- The public retry bound has one owner in `display_limits.h`, and
  `transaction_types.h` now directly includes it. All 15 installed Display
  public headers independently passed first-include syntax compilation. The
  staged prefix contains exactly four Display static libraries and 15 Display
  public headers; the standalone `transaction_types.h`-first consumer builds
  and exits 0.
- Repository documentation validation passed 51 pages and navigation; source
  shape passed 885 files with zero allowlists. I did not independently replay
  strict MkDocs because `mkdocs` is not on this review shell's PATH; Kellan's
  handoff records its successful strict run.

Independent focused execution against the candidate build products:

| Gate | Result |
| --- | --- |
| Strict Debug Display CTest | 11/11 passed, exit 0 |
| Strict Release Display CTest | 11/11 passed, exit 0 |
| ASan+UBSan Display CTest (`detect_leaks=1`, halt on error) | 11/11 passed, exit 0 |
| Installed standalone consumer | exit 0 |
| Independent same-revision adversarial probe | reproduced P1, compile/run exit 0 |

No display, Wayland/XWayland, D-Bus/session, GUI, input, hardware, host service,
or nested runtime ran. Those remain D0/D2/D8 boundaries and are not claimed as
D1 evidence. The passing gates do not override the executable lineage defect;
`0a8d0e0e` is not acceptable for integration.

