# Faraday Stone — Bluetooth B0 exact rereview PASS

- Time: 2026-08-28T20:34:00Z
- Reviewer: Faraday Stone, Z.AI coding plan `zai-coding-plan/glm-5.3`, reasoning high
- Exact candidate: `278a5f9520f3cc47e554816961c0c653295fcbc4`
- Tree: `8c520c26a31a60fa54c3bf5165d3a7ca1fc8ba95`
- Sole parent: `f810108b4042b2215a318f48430de743b883d51a` (my prior FAIL base)
- Diff confinement: 5 paths, +24/-15, exactly the claimed P1-1 fixture
  repair plus P3 polish (transport test, protocol test, test CMake
  `QINDAQT_DBUS_DAEMON_EXECUTABLE` consolidation, one cosmetic client brace,
  bluetooth-service.md heading/status); no production logic change beyond
  the cosmetic brace
- Worktree: detached and clean (`git status --porcelain` empty) before,
  during (SHA-256-proven after every poison/restore), and after review;
  final tree hash equals `8c520c26…`
- Verdict: **PASS for integration and QQ-005.05 advancement**
- Findings: **P0/P1/P2/P3 = 0/0/0/1**

The prior blocking P1-1 is genuinely closed in source and in executable,
mutation-proven evidence. Every prior P3 polish item is verified closed.

## P0 — none

Review touched no host D-Bus/BlueZ/rfkill/radio/GUI/config surface; all bus
work ran on test-spawned private `dbus-daemon` instances; staged installs
went into test-owned build-tree prefixes.

## P1 — none (prior P1-1 attacked and held)

Independent attack on the repaired hostile oversized-wire row:

1. Address set recomputed independently: all 257 addresses
   (`AA:BB:CC:33:00:00` … `AA:BB:CC:33:01:00`) are canonical 17-char
   uppercase-hex D-Bus strings, all unique, aggregate 257 > 256 while every
   per-device field validates. The prior `invalid-device` masking from
   index 44 is gone; only the wire bound distinguishes the payload.
2. Poison 1 — bypassed `wireValid = false` in `readBoundedArray`
   (bluetooth_dbus.cpp), rebuilt: registered row
   `hostileOversizedWireSnapshotIsRejected` FAILS exit 1, Actual
   `client.state()` 2 (Ready) vs Expected Unavailable (3) at
   tst_qt_bluetooth_transport.cpp:273. Restored byte-exact (SHA-256
   `2487ec7b…` match), rebuilt: 4/4.
3. Poison 2 — removed the early `!wireValid` rejection in
   `validateSnapshot` (bluetooth_validation.cpp): same row FAILS exit 1,
   Ready vs Unavailable. Restored (SHA-256 `7275dd36…`), 4/4.
4. Poison 3 — removed `devices.size() > kMaxDevices` from the validator:
   the candidate's new defense row fails correctly, exit 1, Actual reason
   `""` vs Expected `oversized-payload` at tst_bluetooth_protocol.cpp:331
   (the new `wireValid = true` assertion catches a count-bound regression
   the old manual-flag row could not). Restored, 16/16.

The bound is now enforced by two independent, individually load-bearing
layers (codec flag + validator count), each with a registered failing
mutation, plus the client's authority revocation assertions.

## P2 — none

All five prior P2 closures are untouched by this diff and re-verified
executable: lease fail-closed + unique-name release (lease-owner-loss row),
already-connected preflight, public contracts/decomposition, queued async
transport with bounded backoff, full-fidelity wire types with battery/role.

## P3 — one bounded integration note (not a candidate defect)

- `git merge-tree origin/main 278a5f9` (main `146fc483`, exit 1) still
  conflicts only in `docs/wiki/adr/index.md` and `mkdocs.yml`; source and
  test registries merge cleanly. Manager-side resolution as previously
  recorded: retain both public ADR/nav additions at integration.

Prior P3s are closed: duplicate `## Qualification boundary` heading removed
(single heading verified); Qualification-boundary wording now truthfully
matches the executing evidence and bounds the unqualified hardware/session
gates; PrivateBus uses the configured `QINDAQT_DBUS_DAEMON_EXECUTABLE` with
the `find_program` hoisted before its first use; `bluetooth_client.cpp:400`
brace formatting cleaned.

## Source-policy poisons on this exact tree (all restored byte-clean)

- Device-writer cross-type reorder (signature `ssu…`→`sus…`): protocol rows
  `fixedSignatures` and `realWireMarshallingMatchesCanonicalSignatures`
  FAIL exit 2.
- Device-writer same-type address/name swap (signature-invisible): caught
  by the real-bus full-fidelity round trip — transport FAIL exit 1,
  Unavailable vs Ready.
- Model lease-cap preflight lifted: `boundsDiscoveryLeases` and
  `dispatchedLeasesReserveCapacity` FAIL exit 2.
- Observation (not a finding): the publication-side `leaseBoundsRespected`
  total-cap backstop is unreachable through the public API because the
  preflight and backend layers refuse first — fail-closed layering,
  unchanged from the accepted prior review.

## Verification record (commands, exits, counts)

| Gate | Command (abbreviated) | Exit | Result |
| --- | --- | --- | --- |
| Identity | `git rev-parse HEAD HEAD^{tree} HEAD^` | 0 | candidate/tree/parent byte-exact |
| Diff hygiene | `git diff --check f810108 278a5f9` | 0 | clean; 5 paths +24/-15 |
| Configure strict Debug | `cmake -S . -B …/dev -DCMAKE_BUILD_TYPE=Debug -DCMAKE_AUTOMOC_PATH_PREFIX=ON -GNinja` | 0 | configured |
| Build 9 Bluetooth targets (Debug) | `ninja -C …/dev <8 test executables + qindaqt-bluetooth-service>` | 0 | 0 warnings |
| Debug focused suite | `ctest -R bluetooth --output-on-failure` | 0 | 9/9 incl. real whole-tree staged-install row (I completed adjacent install artifacts in my own build dir; no candidate change) |
| Direct totals (Debug) | each of 8 executables | 0 | 16+17+11+12+4+3+4+3 = 70 passed, 0 failed |
| Configure strict Release | `cmake -S . -B …/release … -DCMAKE_BUILD_TYPE=Release` | 0 | configured |
| Build 9 targets (Release) | `ninja -C …/release <same 9>` | 0 | 0 warnings |
| Release focused suite | `ctest -R bluetooth` | 0 | 9/9 incl. staged-install row with linked installed consumer |
| Direct totals (Release) | each of 8 executables | 0 | 70 passed, 0 failed |
| Negative controls ×6 | codec flag bypass; validator wire-flag removal; validator count-bound removal; writer cross-type reorder; writer same-type swap; lease-cap preflight lift | 1/1/1/2/1/2 | every control FAILS as required; each restored byte-exact and re-verified green |
| Source shape | `python3 tools/check-source-shape` | 0 | all files within bounds |
| Docs | `python3 tools/validate-docs` | 0 | 66 documents + nav |
| Strict MkDocs | `/home/cabewse/venv/bin/mkdocs build --strict` (pinned 1.6.1) | 0 | built clean, tree left clean |
| Collision | `git merge-tree origin/main 278a5f9` | 1 | two known additive conflicts (P3 note) |
| Final clean proof | `git status --porcelain`; `git rev-parse HEAD^{tree}` | 0 | empty; `8c520c26…` |

## Caveats (bounded, unchanged)

- No BlueZ, real adapter, rfkill, suspend/resume, hotplug, or hardware
  qualification — B0 boundary.
- UI (Settings view model, applet) remains unimplemented by design.

## Next action

Manager integrates candidate `278a5f9520f3cc47e554816961c0c653295fcbc4`
(non-amended descendant of `f810108b…`, Gemini-authored, this PASS is the
required independent non-Gemini acceptance), reruns the affected gates on
the integrated tree, and resolves the two additive ADR/nav conflicts by
retaining both public additions. QQ-005.05 may advance on integrated
evidence.
