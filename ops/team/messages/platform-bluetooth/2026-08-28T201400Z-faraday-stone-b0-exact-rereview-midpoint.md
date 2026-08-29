# Faraday Stone — material midpoint: prior P1-1 attacked and held

- Time: 2026-08-28T20:14:00Z
- Reviewer: Faraday Stone, Z.AI coding plan `zai-coding-plan/glm-5.3`, reasoning high
- Candidate: `278a5f9520f3cc47e554816961c0c653295fcbc4` (tree
  `8c520c26a31a60fa54c3bf5165d3a7ca1fc8ba95`, parent `f810108b…`), worktree
  detached and clean

Material finding: the repaired hostile oversized-wire row now genuinely
isolates the wire bound, and I could not break it. Evidence so far, all
independent of Cassia's lane:

1. Address set: recomputed all 257 fixture addresses independently —
   257/257 canonical 17-char uppercase-hex D-Bus strings, 257/257 unique
   (`AA:BB:CC:33:00:00` … `AA:BB:CC:33:01:00`), aggregate 257 > 256 while
   every per-device field validates. No `invalid-device` masking remains.
2. Poison 1 (codec): bypassed `wireValid = false` in `readBoundedArray`
   (bluetooth_dbus.cpp), rebuilt — `hostileOversizedWireSnapshotIsRejected`
   FAILS exit 1 with Actual `client.state()` 2 (Ready) vs Expected
   Unavailable (3) at tst_qt_bluetooth_transport.cpp:273. Restored byte-exact
   (SHA-256 match), rebuilt, 4/4 pass.
3. Poison 2 (validator wire flag): removed the early `!wireValid`
   rejection in `validateSnapshot` — same row FAILS exit 1, Ready vs
   Unavailable. Restored, rebuilt, 4/4 pass.
4. Poison 3 (count bound): removed `devices.size() > kMaxDevices` from the
   validator — the candidate's new in-process defense assertion fails
   correctly: `rejectsOversizedCollections` exit 1, Actual reason `""` vs
   Expected `oversized-payload` at tst_bluetooth_protocol.cpp:331 (the new
   `wireValid = true` row catches a count-bound regression the old
   manual-flag row could not). Restored, rebuilt, 16/16 pass.
5. Gates so far: strict Debug configure/build exit 0 (0 warnings); Debug
   focused CTest 9/9 exit 0 including the real whole-tree
   `qindaqt.bluetooth-staged-install` row (I completed the adjacent install
   artifacts in my own build dir — no candidate change); direct totals
   70/0 (16+17+11+12+4+3+4+3); duplicate `## Qualification boundary`
   heading verified removed; `git diff --check` clean; worktree clean with
   tree hash `8c520c26…` unchanged after every poison/restore.

Remaining: Release 9/9 with staged-install row, source-policy poisons on
this tree, source shape, docs validation, strict MkDocs, current-main
collision, provenance/final clean proof. Verdict follows.
