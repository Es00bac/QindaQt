# Display D1 exact candidate handoff

- **Timestamp:** 2026-08-27T19:27:44-06:00
- **From:** Display D1 lead
- **To:** Manager and different-worker reviewer
- **Required base:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- **Exact candidate:** `0e38fa726af69e34be3cacdd6b71d40350ac8092`
- **Subject:** `Establish deterministic Display1 foundation`
- **State:** candidate committed; independent review required before integration

## Delivered outcome

The candidate adds the four pure D1 modules and their public boundaries:

- `display_protocol`: versioned bounded values, total semantic validation,
  canonical candidate/snapshot codecs, and fixed QtDBus value serialization;
- `display_identity`: privacy-preserving precedence, explicit ambiguity,
  connector fallback, schema-v2 registry values, v1 migration, aliases, and
  deterministic reconciliation;
- `display_topology`: normalization, exact logical rounding, transforms,
  mirrors, overlap/gaps, coordinate bounds, canonical fingerprints, diffs,
  and no-op truth; and
- `display_transaction`: injected monotonic clock/side-effect port, one active
  transaction, journal value/codec, apply/observe/confirm/cancel/deadline,
  uncertainty, three total rollback attempts, settle-gated hotplug recovery,
  survivor-only restoration, no-fight external intent, cleanup-only journal
  failure, and observable terminal/Stuck truth.

It also adds 11 focused selectors, the Display architecture/reference pages,
ADR-0015/0016, and the smallest additive source/test/docs navigation changes.
The exact commit has 66 files and 8,545 inserted lines.

## Exact verification

| Gate | Verdict |
| --- | --- |
| Strict Debug focused | build exit 0; CTest 11/11, exit 0 |
| Strict Debug broad | build 756/756, exit 0; CTest 119/119, exit 0 |
| Strict Release focused | build 77/77, exit 0; CTest 11/11, exit 0 |
| Strict Release broad | build 749/749 remaining, exit 0; CTest 119/119, exit 0 |
| ASan+UBSan focused | build 77/77, exit 0; CTest 11/11, exit 0 with leak/halt-on-error |
| Documentation | `validate-docs` 51 pages, exit 0; MkDocs strict, exit 0 |
| Source/static | source-shape 885 files, exit 0; staged diff check exit 0; dependency audit clean |

## Bounded caveats and requested review

This is deterministic `Q-det` evidence only. D2 still owns the resident D-Bus
service/name/XML, positive inbound private-bus demarshalling proof, real clock
and timer, filesystem journal, lock/logind/Wayland adapter, compositor
mutation, and nested hotplug/restart convergence. D8 owns physical hardware
qualification. No host state was changed.

The only untracked worktree path is external
`ops/team/workers/kai-mercer.md`; it is not in the candidate.

Please review **exactly**
`0e38fa726af69e34be3cacdd6b71d40350ac8092` against all seven queued D1
contracts and the accepted Fable decision. Recheck the final transaction
counterexamples (attempt-budget idempotence, settle barrier, set flap/full
pre-image, cleanup-only journal failure, live snapshot projection, external
no-fight recovery, Stuck topology adoption), D-Bus direction/signature truth,
identity privacy/ambiguity, topology rounding/fingerprints, dependency purity,
and docs/API ownership/lifetime/thread/error claims. Return exact defects or an
evidence-backed acceptance; any repair requires a new commit and rereview.
