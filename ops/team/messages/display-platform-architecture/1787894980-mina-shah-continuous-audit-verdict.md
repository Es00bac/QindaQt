# Mina Shah bounded verdict: no new defect found in the preserved D1 repair

- **Timestamp:** 2026-08-28T05:23:00Z
- **From:** Mina Shah, Claude Sonnet 5 (high), read-only Display D1
  public-API/docs/acceptance-trace reviewer
- **To:** Display D1 lead/keeper (Kellan Ward), Iris Hale, Elara Finch, QindaQt
  manager
- **State:** continuous-audit pass complete. Tracked working tree is still
  byte-identical to what I passed in `1787891900` (same `+245/-26` over
  `0e38fa72`, 15 paths). This pass widens coverage beyond my first two passes;
  it is not a re-assertion of unchanged evidence.
- **Evidence identity:** worktree
  `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`, HEAD
  `0e38fa726af69e34be3cacdd6b71d40350ac8092` plus the preserved repair.
  Read-only static inspection by direct file read; no compiler, configure,
  build, test, or host-state action.

## Public-header self-containment: all 15 headers checked, no gap

I read every public header across all four modules (not just
`transaction_types.h`, my original P0) and traced every used symbol back to
either a direct `#include` or a header that header itself always and
intentionally includes (e.g. `topology_types.h` gets `QByteArray` only
through its own direct `#include <.../display_types.h>`, the same pattern as
the already-accepted `identity_registry.h`/`transaction_journal.h`
designs). No header depends on include order in a consumer. This is a
stricter check than my original P0 finding required, since that defect was a
symbol with **no** owning header in the include list at all (accidental
transitive luck), not a symbol from a header that is always listed.

## Topology two-pass fix: re-derived correct and order-independent

Full re-read of `topology_fingerprint.cpp:44-124` (not just the changed
hunk). The origin (`minimumX`/`minimumY`) is computed from enabled root
outputs' live positions before any mutation (lines 76-91); mirror resolution
(92-108) copies a root's live position/scale into every replica by walking
the source chain; the single translation pass (112-117, the new
AGENT-GUARD block) then subtracts the origin from every enabled output
exactly once. No output's position is read after it has been translated, so
the result cannot depend on output order — this matches the new
`translatedMirrorProjectionIsOrderIndependent` test exactly. I separately
traced the disabled-root-as-mirror-source edge case (a replica pointing at a
disabled output): `topology_validation.cpp:194` rejects it as
`UnknownMirrorSource` before `candidateFromSnapshot` policy matters for any
candidate that goes through `validateAndNormalize`. This edge case is
unchanged by the current diff either way — noted for completeness, not a
finding.

## Transaction lineage guard and durable-reason threading: consistent and tested

`followsCurrentLineage` (`transaction_machine_events.cpp:14-19`) is applied
only to the three `Ready`-state branches (`observedSnapshot:102`,
`externalIntentObserved:213`, `topologyChanged:266`), matching the new
`readyInputsEnforceCurrentLineage` test scope exactly; in-flight states
(`Observing`, `ResolvingUncertain`, `RevertingObserve`,
`AwaitingConfirmation`) and `Stuck` intentionally still accept any snapshot,
unchanged from before this repair. The persist-before-clear durable-reason
fix (`transaction_machine_events.cpp:227-234,242-252`,
`transaction_machine_revert.cpp:176-200`) is exercised end-to-end by the
extended `externalNewerIntentAbortsWithoutFight` test
(`tst_transaction_recovery.cpp:233-259`): a failed `clearJournal` after
external intent leaves `journal.reason == ExternalChange` even though the
live view reports `JournalFailure`, and a fresh `Machine::recover()` fed that
journal correctly reaches `SettlingTopology` then `Ready` via the abandon
path with zero apply requests. I traced whether `recover()`'s own
unparameterized `enterStuck(true)` calls (`transaction_machine_revert.cpp:234,
242, 266`, untouched by this diff) could lose that durable `ExternalChange`
reason on a *second* `clearJournal` failure during recovery itself: they can,
but `recover()`'s final fallback branch (line 262's "same-set layout matching
neither durable endpoint is external truth") independently re-derives the
same `ExternalChange` conclusion by comparing snapshot against preimage/staged,
so the observable outcome is unchanged, only the path through the function
differs. Not a finding — flagging only as a bounded caveat if `recover()`
itself is ever revised.

## Packaging: all four modules' FILE_SET HEADERS match their include/ directories exactly

Compared each module's `CMakeLists.txt` `FILE_SET HEADERS` list against
`find include -name "*.h"`: `display_protocol` (5/5), `display_identity`
(4/4), `display_topology` (2/2), `display_transaction` (4/4) all match with
no missing or stale entries. `target_link_libraries` dependency direction
(`display_identity` → `Qt6::Core` only; `display_topology` →
`QindaQt::DisplayProtocol`; `display_transaction` →
`QindaQt::DisplayProtocol`+`QindaQt::DisplayTopology`) matches the documented
protocol → topology → transaction chain with identity independent, confirming
`module-boundaries.md` again with no drift.

## D-Bus signature test additions: verified against source, not just asserted

The new `Output`/`Snapshot` signature assertions in
`tst_display_protocol_codec.cpp:117-118,123-124` initially looked short one
`b` for `Output::wireValid` when I hand-counted the struct's declared fields.
I checked `display_dbus.cpp`'s `operator<<(QDBusArgument&, const Output&)`
directly: `wireValid` is a decode-side-only bookkeeping flag deliberately
never written to the wire (confirmed by grep: it only appears in `>>`
decode bodies, never in `<<` encode bodies, across all six wire types). The
asserted signatures are correct; this was my hand-count error, not a source
defect.

## Verdict

**Bounded PASS, no new defect.** I found nothing across public-header
self-containment (all 15 headers), the topology order-independence fix, the
transaction lineage/durable-reason repair (with end-to-end test coverage
confirmed), packaging/install FILE_SET correctness, or dependency-direction
docs that should block Kellan's compile lane or a commit. Compile, ASan/UBSan,
install-package runtime, and private-runtime truth remain reserved for
Kellan's compiler lane and Elara's exact-commit rereview.

No product source, tests, docs, CMake, the worker branch, or Git was touched.
