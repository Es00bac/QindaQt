# Tessa Rowan — Controls S2 repaired exact-candidate verdict: FAIL

Timestamp: 2026-08-27T23:04:44-06:00

## Exact identity and verdict

- Candidate: `5be6df91b8aa2a06fc5c07bef44d39857094e088`
- Tree: `000e58c658f8d17e896d2b88a7c1266bc8d5831c`
- Sole parent/rejected candidate:
  `10996f146ff78f69a6f1019933d812d1475faf85`
- Detached review worktree: clean, exact, no branch
- Diff: 14 paths, Controls source/build/tests and two owning wiki pages; no
  production shell/service/display path
- Verdict: **FAIL — P0/P1/P2/P3 = 0/0/1/1**

Do not integrate this commit. The original implementer, Cora Vale, must create
one new non-amended descendant and return its exact commit/tree to Tessa for
rereview.

## Blocking P2 — the private coalescer broke the real Qt accessibility path

`src/controls/qml/StateCard.qml:52-76` moved `publishLatest()` into the private
`QtObject` named `announcementState`, then invokes the unqualified attached
method `Accessible.announce(announcement, politeness)` from that object's
lexical scope. Qt therefore tries to attach `Accessible` to the `QtObject`, not
the root `T.Control`, and rejects the call because the target is neither an
Item nor an Action.

The shortest in-tree reproduction is already built into the candidate:

```text
env TMPDIR=<worktree-local> XDG_RUNTIME_DIR=<private-0700> \
  ctest --test-dir build/tessa-controls-debug \
  -R '^qindaqt\.controls-behavior$' --verbose --parallel 1
```

The command exits 0 with 19/19 QtTest functions passing, but the exact dynamic
announcement function prints:

```text
QWARN ... StateCard.qml:52:5: QML QtObject: Accessible attached property must
be attached to an object deriving from Item or Action
```

This is a false-green: `control.accessibilityAnnouncementRequested(...)` at
`:77-78` still emits, so `state_card_accessibility_test.cpp:19-44` observes the
paired mirror tuple and passes even though the preceding real
`Accessible.announce` call is invalid. The wiki explicitly says that signal is
not a substitute AT bridge, so the candidate violates its public accessibility
contract.

An independent temporary qmltestrunner probe against the clean installed stage
also passed its construction-silence and later same-status-title tuple checks
3/3 while emitting the identical `QML QtObject` warning. The probe was outside
the candidate and was deleted after reproduction.

Required closure:

1. Preserve the private next-event coalescer, but invoke the attached method on
   the root Control (for example, explicitly through the root Control's
   `Accessible` attached object) rather than lexically on the `QtObject`.
2. Keep the paired signal byte-for-byte aligned with the real call and retain
   the complete latest tuple, urgency, construction-silence, and no-duplicate
   guarantees.
3. Make the committed behavior regression fail on this exact attached-property
   warning (or directly witness the real Qt accessibility event), so a passing
   mirror signal cannot mask a rejected real call.

## P3 — construction silence has no retained direct observer

The wiki promises construction does not announce. The committed helper creates
the scene before installing its `QSignalSpy`
(`tst_controls_behavior.cpp:328-329`,
`state_card_accessibility_test.cpp:17-20`), so it cannot observe a construction
announcement. My temporary staged probe dynamically created the card, attached
before the next event turn, and confirmed the present readiness gate is silent;
the implementation is currently correct, but the regression is not retained.
Add the equivalent direct construction observer to the committed focused test
while repairing the P2.

## Independently passing evidence before the stop

- Fresh Debug configure: exit 0. Fresh full serial install-capable build:
  1,308/1,308 steps, exit 0. Host-uinput registration was explicitly off.
- Debug exact discovery: 29 Controls rows. Debug serial selector: 29/29 passed
  in 15.54 seconds. The aggregate pass is not acceptance because its behavior
  row hides the blocking QWARN above.
- Detailed Debug package/PSS selector: 2/2. The staged package has exactly the
  14 generated `qml/*.qml` files; strict staged qmllint and runtime import pass.
- `readelf` proves the staged backing library directly `NEEDED`
  `libqindaqt_tokens_qml.so` with `RUNPATH [$ORIGIN/../Tokens]`; the plugin
  directly needs the backing library with
  `RUNPATH [$ORIGIN:$ORIGIN/../lib]`. `ldd` resolves Tokens from the sibling
  staged module.
- Repeated Debug PSS evidence: median bare 17,971 KiB, Controls 39,475 KiB,
  paired delta 21,511 KiB across three pairs; `threshold` remains `null`. This
  is measurement only, not a budget pass.
- Exact ancestry/tree, 14-path scope, whitespace, queried QML install loop,
  exact positive import allowlist, both named font witnesses, ADR-0021 wording,
  private readiness/revision state, latest-tuple Timer semantics, module
  dependency direction, public ownership/lifetime/thread/error/compatibility
  prose, keyboard/focus behavior, 25-row visual identity, and package inventory
  are otherwise closed by source audit plus the executed Debug evidence.
- Both former installed-module P2 requirements and all three former P3 repairs
  are closed. This is one new P2 introduced by the StateCard privacy repair and
  one retained-test P3 exposed by the requested construction audit.

## Stopped work, cleanup, and manager action

Per the stop-on-blocker rule, Release configure/build/CTest and remaining broad
static gates were not run; no Release claim is made. The unrelated previously
recorded shell production-surface timeout was not rerun, excused, or claimed
passing. No broad registry pass is claimed.

The temporary probe file, exact private runtime root, and worktree-local
compiler temp were removed; the detached candidate remains clean; no Tessa
compiler/test/runtime process remains. Another owner may use the serial lane.

Manager action: reject `5be6df9`, route this exact reproduction and repair set
to Cora Vale, preserve the commit, and return Cora's new non-amended descendant
to Tessa Rowan for exact rereview.
