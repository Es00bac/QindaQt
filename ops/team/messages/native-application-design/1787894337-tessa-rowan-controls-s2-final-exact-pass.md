# Controls S2 final exact-candidate PASS

- Author: Tessa Rowan
- Time: 2026-08-27T23:18:57-06:00
- Verdict: **PASS**
- Findings: P0/P1/P2/P3 `0/0/0/0`
- Candidate: `e774dac00166d42d7b84cae957944c22f70b02db`
- Tree: `86e8b3ec26ebd62362a0c10ec700d3c5da467afe`
- Parent series: `a52efb7` -> `5be6df9` -> `10996f1` -> `a083a20`

The exact two-commit repair is bounded to `src/controls/qml/StateCard.qml` and
three focused Controls test files. `control.Accessible.announce(...)` now
resolves the API against the root `T.Control`; the test installs a real
QAccessible update handler before scene construction, requires zero construction
announcements, makes the former invalid-attached-property warning fatal, and
compares the real announcement's source object, message, and politeness with
the mirror tuple across normal, alert, same-status-content, and final-tuple
coalescing transitions. The earlier staged package/tooling repair remains
unchanged.

Independent evidence at the exact candidate:

- `git diff --check 5be6df9..e774dac`: exit 0.
- Fresh serial Debug incremental build of
  `qindaqt_controls_behavior_tests`: 11/11 affected steps, exit 0.
- Verbose Debug `^qindaqt\.controls-behavior$`: 1/1 CTest row and 19/19 QtTest
  functions, exit 0; the real-event/warning-negative/construction-silence probe
  passed with accessibility forced on and the offscreen software backend.
- Full Debug `^qindaqt\.controls-`: 29/29, exit 0.
- Independent exact Release `^qindaqt\.controls-` execution from Cora's clean
  candidate build: 29/29, exit 0.
- Both full selectors include behavior, 25 visual rows, source policy, measured
  PSS, and staged installed import/tooling.
- Detached review checkout is clean at the exact tree; no review compiler,
  test, runtime, or temporary process remains.

No blocking or follow-up finding remains in the assigned boundary. The manager
should integrate the exact four-commit Controls series through `e774dac` now,
rerun the affected integrated gate, and advance the integrated Controls outcome
evidence rather than counting this candidate handoff itself.
