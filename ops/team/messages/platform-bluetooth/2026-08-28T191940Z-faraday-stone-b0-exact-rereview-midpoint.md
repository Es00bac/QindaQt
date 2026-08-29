# Faraday Stone — Bluetooth B0 rereview midpoint (material finding)

- Time: 2026-08-28T19:19:40Z
- Reviewer: Faraday Stone, GLM `zai-coding-plan/glm-5.3`, reasoning high
- Exact candidate: `f810108b4042b2215a318f48430de743b883d51a` (tree
  `21f2dbcfdacdd05cce922f3c950cbabfd15bb4f3`, sole parent
  `e19d094c792d132d3d65129056281ca556415c0f`), read-only worktree clean
  throughout.

## Material finding (blocking-evidence class, negative-control reproduced)

**The hostile oversized-wire row does not isolate the bounded decode.**
`tst_qt_bluetooth_transport.cpp::hostileOversizedWireSnapshotIsRejected`
builds its 257-device payload with
`QStringLiteral("AA:BB:CC:33:44:%1").arg(56 + index, 2, 10, QLatin1Char('0'))`
(tests/services/bluetooth_client/tst_qt_bluetooth_transport.cpp:122-123).
From index 44 the last octet becomes three decimal digits (`"100"`), so 213
of the 257 hostile devices carry **non-canonical addresses** and
`validateSnapshot` rejects the payload as `invalid-device` through address
validation, regardless of the array-bound path.

Exact reproduction (in a scratch copy under `/tmp/opencode`, never the review
tree): delete the `else { wireValid = false; }` branch of `readBoundedArray`
(src/services/bluetooth_protocol/src/bluetooth_dbus.cpp:24-40), rebuild
`qindaqt_bluetooth_qt_transport_tests`, run → **exit 0, 4 passed** — the
entire claimed hostile-wire row passes with the bounded decode removed. The
prior Lovelace P1-9 item "decode an oversized wire array on the real wire" is
therefore still not evidenced: a regression that silently truncates oversized
arrays instead of returning `oversized-payload` would not be caught by any
row (the protocol test sets `wireValid = false` manually; no other test
decodes a over-bound, otherwise-valid payload from the bus). Proposed repair:
generate canonical addresses across all 257 devices (hex octets or a second
adapter prefix) so only the array bound distinguishes the hostile payload,
and keep the rest of the row unchanged.

## Verification so far (module-first per manager correction)

- Strict Debug Bluetooth targets (protocol/model/client/service + 9 test
  executables + staged consumer): ninja exit 0, 0 warnings.
- `ctest -R bluetooth` Debug: **9/9 passed, exit 0** (includes whole-tree
  staged install + linked installed consumer).
- Release Bluetooth targets: ninja exit 0. `ctest -R bluetooth` Release:
  8/9; the single failure is the staged-install row requiring whole-tree
  install artifacts the cancelled full build had not produced — my
  proportional-build artifact, not a candidate defect. Bluetooth Release
  payload verified instead by running the candidate's own generated
  per-module install rules into an isolated stage (executable, activation
  descriptor, unit, introspection XML, protocol archive; no `@...@`
  placeholders) plus configure/build/run of the installed consumer: exit 0.
- Direct totals (Debug, per executable): protocol 16/16, model 17/17,
  deterministic backend 11/11, client 12/12, qt-transport 4/4, service 4/4,
  lease-owner-loss 3/3, activation 3/3 — 0 failed.
- Negative controls (scratch copy): ABI writer reorder → protocol tests FAIL
  (exit 2); total-lease cap removal → model tests FAIL (exit 1);
  unique-name caller-loss skip → lease-owner-loss FAIL (exit 1);
  activation-path removal → activation test FAIL (exit 1); **bounded-decode
  removal → hostile row still PASSED (the finding above)**.
- Static gates: `tools/check-source-shape` exit 0 (largest Bluetooth file
  484 lines, none over 500); `tools/validate-docs` exit 0 (66 documents);
  pinned `mkdocs==1.6.1` `mkdocs build --strict` exit 0; `git diff --check`
  clean.
- Collision state: `git merge-tree origin/main f810108` still conflicts in
  `docs/wiki/adr/index.md` and `mkdocs.yml` (source/test registries merge
  cleanly) — manager-side additive resolution, unchanged from the prior
  review note.

All other Lovelace P1/P2/P3 items verified closed in source plus executable
evidence; full ledger with counts follows in the final verdict. Remaining
bounded notes so far: duplicated `## Qualification boundary` heading
introduced in `docs/wiki/architecture/bluetooth-service.md:128-130`, stale
"not yet built" status wording, cosmetic brace wart in
`bluetooth_client.cpp:400`, duplicated PrivateBus fixture with literal
`dbus-daemon` in `tst_qt_bluetooth_transport.cpp`.
