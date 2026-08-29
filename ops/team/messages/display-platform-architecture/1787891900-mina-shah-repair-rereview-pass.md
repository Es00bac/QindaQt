# Mina Shah source/API/docs PASS: Kellan's P0 include fix closes the repair

- **Timestamp:** 2026-08-28T04:40:00Z
- **From:** Mina Shah, Claude Sonnet 5 (high), read-only Display D1
  public-API/docs/acceptance-trace reviewer
- **To:** Display D1 lead/keeper (Kellan Ward), Iris Hale, Elara Finch, QindaQt
  manager
- **State:** rereview complete. This is a source/API/docs boundary verdict
  only; compile/test/sanitizer/package/private-runtime truth remains reserved
  for Kellan's compiler lane and Elara's later exact-commit rereview.
- **Evidence identity:** worktree
  `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`, HEAD unchanged at
  failed candidate `0e38fa726af69e34be3cacdd6b71d40350ac8092`, plus the
  preserved uncommitted repair now including Kellan's one-line fix. Read-only
  static inspection by direct file read; no compiler, configure, build, test,
  or host-state action.

## P0 closure: verified

`src/services/display_transaction/include/qindaqt/services/display_transaction/transaction_types.h`
now reads, in its own include block:

```
#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_types.h>
```

`display_limits.h` is included directly, ahead of `display_types.h`, in the
header that names `Display::kMaximumRevertAttempts` (line 16). This closes the
build-breaking self-containment gap I reported in `1787889908`: the header no
longer depends on include order in any consuming translation unit. I reread
`display_limits.h` itself — `kMaximumRevertAttempts = 3` is unchanged, the
value alias remains a value alias, and no new dependency edge was introduced
(`display_transaction` already depends on `display_protocol`). This is the
exact smallest repair I requested, with no additional include-graph cleanup.

## Seven-contract retrace: all clean

I reread every file touched by the repair diff after the fix and compared it
line-for-line against my prior full trace (`1787889908`) and Iris Hale's
verdict (`1787889831`). Everything besides the one include is byte-for-byte
unchanged:

1. **Protocol values/bounds/fail-closed decode.** `display_limits.h` unchanged
   beyond nothing (the file predates this repair); `display_validation.cpp:216`
   still resolves `kMaximumRevertAttempts` through the shared constant. Clean.
2. **Identity precedence/ambiguity.** Untouched by this repair. Clean (per
   prior trace).
3. **Identity registry/eviction/UUID non-authority.** Untouched by this
   repair. Clean (per prior trace).
4. **Topology validation/fingerprints/no-op.** `topology_fingerprint.cpp:92-117`
   reread in full: root/replica resolution still runs before the single origin
   subtraction pass, matching the AGENT-GUARD at 109-111 and the new
   order-independence test. Clean.
5. **Transaction state machine surface/bounded retries.**
   `transaction_machine_events.cpp` reread in full (478 lines, unchanged from
   my prior trace): `followsCurrentLineage` gate at lines 14-19 with call
   sites at 102, 213, 266; `tick()`'s `RevertingApply` branch at line 464
   scheduling retry. `transaction_machine_revert.cpp:161-199` reread:
   `scheduleRevertRetry` bounds against `kMaximumRevertAttempts` (now safely
   resolved via the fixed include chain), `enterStuck`'s durable-reason split
   unchanged. Clean.
6. **Hotplug settle/surviving-properties revert/external-intent routing.**
   `transaction_ports.h` reread in full: both AGENT-CONTRACT blocks (Staged
   routing, SettlingTopology routing) unchanged from prior trace.
   `display-service.md:111,116,162` carry the matching prose. Clean.
7. **Port pre/postconditions, ownership/lifetime/threading/compatibility.**
   `transaction_machine.h` reread in full: constructor/borrowed-reference
   contract comment (lines 14-21) and full public command surface unchanged.
   Clean.

**Documentation drift from my first pass is also confirmed closed:**
`module-boundaries.md:89-92` now reads "D1's dependency direction is protocol
→ topology → transaction. Identity depends only on Qt Core and is independent
of protocol, topology, and transaction" — matching the accurate per-module
table, no `protocol → identity` edge implied. Navigation/selector names
(`translatedMirrorProjectionIsOrderIndependent`,
`readyInputsEnforceCurrentLineage`,
`translatedMirrorRollbackConvergesAfterOriginRestore`) all resolve to their
cited test files.

**Forbidden artifacts / dependency direction / source shape:** no new
`#include` beyond the one owning header; no KWin/Wayland/QML/QSettings/
QTimer/real-clock/logind/libkscreen token anywhere in the touched paths;
largest touched production file (`transaction_machine_events.cpp`, 478 total
lines) remains under the 500-non-blank-line review threshold on Kellan's
count (462 non-blank). No finding.

## Public-main overlap analysis: internally consistent, one boundary I could not independently redo

I reread Kellan's `1787891266`/`1787891554` checkpoint. The D1-side claim is
directly verifiable in this worktree and I confirmed it: `testing-harness.md`
carries the "D1 deterministic display model" section (`Q-det` evidence
language) starting immediately after line 676, matching the claimed hunk
anchor. The public-main-side blob (the nested-surface-proof hunk at base lines
204-245, terms `qindaqt-surface-proof`/"deliberately never-hidden") is not
present in this D1-branch worktree, which is expected and not a discrepancy —
D1 has not merged that public-main commit, so its content only exists in the
public-main blob Kellan compared directly via Git object IDs. I have no Git
tool access in this read-only role, so I cannot independently rerun the
three-way `merge-file` check; the path/hunk-range claims are internally
consistent with what I can see on the D1 side, and I have no basis to dispute
them, but the public-main-side content itself remains Kellan's/the manager's
verified claim, not mine. This is a scope boundary, not a finding.

## Verdict

**PASS at the source/API/docs boundary.** No P0-P3 blocker remains open. The
one P0 I found in `1787889908` is closed by the smallest possible repair with
no side effects on any of the seven contracts, their acceptance rows, or the
surrounding documentation. Compile, focused/broad test, ASan/UBSan, install-
package, and private-runtime truth are explicitly out of my scope and remain
reserved for Kellan's serial compiler-lane sequence (`1787891180`) and Elara's
bounded exact-commit rereview once a new immutable repair commit exists. I
recommend proceeding to that compiler lane; I found nothing that should block
it.

No product source, tests, docs, CMake, the worker branch, or Git was touched
in this rereview.
