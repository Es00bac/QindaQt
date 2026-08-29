# Faraday Stone — Bluetooth B0 exact-repair rereview FAIL

- Time: 2026-08-28T19:26:00Z
- Reviewer: Faraday Stone, GLM `zai-coding-plan/glm-5.3`, reasoning high
- Exact candidate: `f810108b4042b2215a318f48430de743b883d51a`
- Tree: `21f2dbcfdacdd05cce922f3c950cbabfd15bb4f3`
- Sole parent: `e19d094c792d132d3d65129056281ca556415c0f`
- Manifest: 34 paths, sorted name-status SHA-256
  `0469a7b1e9cb8c7b793167c01eeac4d067fe86185610103bc1a7f0a8038d511e`
  (matches Cassia Rowan's handoff byte-for-byte)
- Worktree: detached, clean (`git status` empty) before, during, and after
  review; candidate untouched
- Verdict: **FAIL for integration and QQ-005.05 advancement**
- Findings: **P0/P1/P2/P3 = 0/1/0/3**

The repair materially closes the Lovelace `0/9/5/3` ledger: nine of nine
prior P1s, all five P2s, and two of three P3s are verified closed in source
**and** executable evidence, with fresh strict Debug/Release builds, direct
per-executable totals, real private-bus rows, staged-install/installed
consumer gates, and mutation-based negative controls. One blocking evidence
defect remains, reproduced by negative control.

## P0

None. The review touched no host D-Bus/BlueZ/rfkill/hardware/session
surface; the candidate contains no destructive or host-mutating path
(deterministic empty backend, no BlueZ contact, hardened user unit).

## P1 — blocking

### P1-1: The hostile oversized-wire row is masked and does not evidence the bounded decode

The claimed closure of prior P1-9 ("decode an oversized wire array") is not
real. `HostileSnapshotService::GetSnapshot` generates device addresses with
`.arg(56 + index, 2, 10, QLatin1Char('0'))`
(tests/services/bluetooth_client/tst_qt_bluetooth_transport.cpp:118-129):
from index 44 the last octet is three decimal digits, so 213 of 257 devices
carry non-canonical addresses and `validateSnapshot` rejects via
`invalid-device` independent of the array bound.

Exact reproduction (scratch copy, review tree untouched): remove the
`wireValid = false` marking from `readBoundedArray`
(src/services/bluetooth_protocol/src/bluetooth_dbus.cpp:24-40), rebuild
`qindaqt_bluetooth_qt_transport_tests` (ninja exit 0), run → **exit 0, 4
passed, 0 failed**. The bounded-decode behavior can regress silently; no row
in the suite would notice (the protocol oversize test sets `wireValid=false`
manually at tst_bluetooth_protocol.cpp:325; no other test decodes an
over-bound, otherwise-valid payload from a real bus).

Repair for Cassia (same worktree, one non-amended descendant): make all 257
hostile addresses canonical (hex octets and/or a second adapter prefix) so
only the array bound distinguishes the payload, assert the client sees
exactly `oversized-payload`-driven rejection (`malformed-snapshot` with no
retained authority), and rerun the row. No production-code change is
required — the codec's bounded decode itself is correct by inspection and by
the manual-flag protocol row.

## P2 — none

All five prior P2s verified closed:

- P2-1 lease inventory fail-closed + unique-name caller identity:
  `leaseBoundsRespected` (bluetooth_model_publication.cpp:123-164) rejects
  unknown-adapter leases, duplicate caller/adapter entries, both lease
  bounds, and discovering-flag contradictions;
  `onNameOwnerChanged` (resident_bluetooth_service.cpp:119-135) releases
  only unique-name disappearance with empty new owner.
- P2-2 already-connected: model preflight (bluetooth_model.cpp:300-302) and
  backend (deterministic_adapter_backend.cpp:254-258), asserted in model and
  transport rows.
- P2-3 public contracts + decomposition: `snapshot()` returns by value with
  documented staleness semantics (bluetooth_model.h:38-41); explicit
  single-thread/lifetime/stop-barrier contracts on model, client, backend
  port; model split at 471+222 lines, largest Bluetooth file 484.
- P2-4 transport async + bounded backoff: failure paths queue emissions
  (qt_bluetooth_transport.cpp:179-195, 216-259); retry doubles 200 ms→2 s
  cap with reset on acceptance (bluetooth_client.cpp:264-274).
- P2-5 battery/role on the wire: Device `((tt)(tt)ssuubbbnby)`,
  `validDeviceRole`/`validBattery` fail-closed
  (bluetooth_validation.cpp:32-45), full-fidelity round trip asserted
  (tst_qt_bluetooth_transport.cpp:192-195), reference page updated.

## P3 — bounded notes

### P3-1: Duplicated heading introduced in the architecture page

`docs/wiki/architecture/bluetooth-service.md:128-130` now contains
`## Qualification boundary` twice in a row. MkDocs strict does not flag it;
fix in the same repair descendant.

### P3-2: Current-main integration still needs the two additive resolutions

`git merge-tree origin/main f810108` (main `146fc483`) conflicts in
`docs/wiki/adr/index.md` and `mkdocs.yml`; source/test registries merge
cleanly. Manager-side resolution unchanged from the prior review: retain
both public ADR/nav additions.

### P3-3: Truth-polish items for the repair descendant

- `bluetooth-service.md` "Current status" still says "not yet built,
  executed, or activated" although Debug/Release focused builds and all
  focused suites now execute (this review and the implementer's lane);
  update to the manager-lane wording before integration.
- `bluetooth_client.cpp:400`: closing brace and `m_operation = ...` share
  one line (cosmetic).
- `tst_qt_bluetooth_transport.cpp` carries a second PrivateBus fixture using
  literal `dbus-daemon` instead of the configured
  `QINDAQT_DBUS_DAEMON_EXECUTABLE` used by the service-suite support header;
  consolidate on the configured path.

Prior P3-3 (user unit `After=bluetooth.service`) is verified closed: the
line is removed and the page states the real no-ordering boundary.

## Prior P1 ledger — closure evidence

1. Canonical signatures: types/writers/XML/classinfo/reference/test all
   `uttuuss`/`ubbbnby`; registered-signature and real-writer
   `currentSignature()` assertions (tst_bluetooth_protocol.cpp:95-125);
   negative control (writer reorder) → protocol tests FAIL exit 2.
2. `sss` owner watch: correct `connect(...SLOT(onNameOwnerChanged(QString,QString,QString)))`
   overload; real lease-owner-loss row on a private bus.
3. Initial publication: backend queues first publish behind `start()`
   return (deterministic_adapter_backend.cpp:42-63); activation row proves
   activated process publishes truthful `Unavailable/no-adapter`.
4. Epoch fencing: 64-bit entropy XOR clock with strict monotone floor;
   reuse-before-publication advances epoch (model test), fresh epochs across
   independent buses (activation test).
5. Lease lifecycle/caps: power-off and stop clear leases; per-adapter and
   total caps with dispatched-lease projection; negative control (total-cap
   removal) → model tests FAIL exit 1.
6. Absent-service activation: `NameHasNoOwner`/`ServiceUnknown` → exactly
   one `StartServiceByName`; negative control (activation removal) →
   activation test FAIL exit 1.
7. Stale-snapshot revocation: failed/timed-out fetch drops the retained
   snapshot and completes dispatched work `Uncertain`
   (bluetooth_client.cpp:315-327, 505-518).
8. Client tests: queued-delivery expectations match source; 12/12 direct.
9. Wire/owner-loss/install evidence: real-bus round trip equals model value;
   private caller-loss release; staged install + linked installed consumer
   (Debug CTest row + Release per-module stage replication) — **except the
   oversize isolation gap that remains P1-1**.

## Verification record (commands, exits, counts)

| Gate | Command (abbreviated) | Exit | Result |
| --- | --- | --- | --- |
| Configure strict Debug | `cmake -S . -B …/dev -DCMAKE_BUILD_TYPE=Debug -DCMAKE_AUTOMOC_PATH_PREFIX=ON -GNinja` | 0 | configured |
| Build Bluetooth targets (Debug) | `ninja -C …/dev <9 bluetooth targets>` | 0 | 0 warnings |
| Debug focused suite | `ctest --test-dir …/dev -R bluetooth --output-on-failure` | 0 | 9/9 passed |
| Direct totals (Debug) | each test executable | 0 | 16+17+11+12+4+4+3+3 = 70 passed, 0 failed |
| Configure strict Release | `cmake -S . -B …/release -DCMAKE_BUILD_TYPE=Release -DCMAKE_AUTOMOC_PATH_PREFIX=ON -GNinja` | 0 | configured |
| Build Bluetooth targets (Release) | `ninja -C …/release <9 targets>` | 0 | 0 warnings |
| Release focused suite | `ctest -R bluetooth` | 8 | 8/9; staged-install row blocked by manager-cancelled whole-tree build artifacts (proportional-build artifact, not a candidate defect) |
| Release staged payload + consumer | candidate per-module `cmake_install.cmake` scripts + installed_consumer build/run | 0 | payload complete, no `@...@`, consumer runs |
| Negative controls ×5 (scratch) | mutations of writer order, total cap, owner-loss, activation, bounded decode | — | first four FAIL as required; bounded-decode mutation NOT caught (P1-1) |
| Source shape | `python3 tools/check-source-shape` | 0 | 0 files over 500 (largest Bluetooth 484) |
| Docs | `python3 tools/validate-docs` | 0 | 66 documents + nav |
| Strict MkDocs | pinned `mkdocs==1.6.1`, `mkdocs build --strict` | 0 | built clean |
| Diff hygiene | `git diff --check e19d094 f810108` | 0 | clean |
| Collision | `git merge-tree origin/main f810108` | 1 | conflicts: adr/index.md, mkdocs.yml (P3-2) |
| Final clean proof | `git status --porcelain` | 0 | empty at `f810108` |

Full-build cancellation: the initial whole-repository Debug build (1613
targets) was deliberately cancelled by manager correction at ~1387/1613 and
is recorded as process efficiency, not a candidate failure; all Bluetooth
evidence above is from proportional module-first builds.

## Caveats (bounded)

- No BlueZ, real adapter, rfkill, suspend/resume, hotplug, or hardware
  qualification — B0 boundary, unchanged.
- The staged-install CTest row was executed whole-tree in Debug only; the
  Release equivalent was replicated per-module (candidate's own generated
  install scripts) because the whole-tree Release install artifacts were not
  built under the proportional mandate.
- UI (Settings view model, applet) remains unimplemented by design.

## Next action

Cassia Rowan repairs P1-1 (plus the P3 polish) in
`/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-b0` as one
non-amended descendant of `f810108`; I remain available to rereview the
exact repaired commit. Integration blocked until then. Gemini-authored work
requires non-Gemini acceptance; this review is that independent lane.
