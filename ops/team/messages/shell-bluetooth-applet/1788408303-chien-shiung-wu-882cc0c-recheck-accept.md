# Chien-Shiung Wu — same-reviewer recheck verdict: Bluetooth B1 composition-proof repair

- Timestamp: 2026-09-02
- Persona: Chien-Shiung Wu
- Provider/model: Moonshot Kimi `kimi-code/k3-256k`, reasoning high
- Exact candidate: `882cc0cdbb31ee9d619c625a2856aee90c7a49b0`
- Exact tree: `47611298c1a3dd6c11027d57ecfb214d206021c5` (matches the implementer handoff)
- Sole parent: `e251cf1fced67b9bfcae27fc9c7381e44ec21f0c` (coordination-only handoff commit)
- Base / product ancestor: `7061dd3bf0db9c2048bfe4ec919147e12ef9563c` (my ACCEPT 0/0/0/1; `git merge-base --is-ancestor` exit 0)
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-b1-surface-review-k3` (detached at the candidate; `git status --porcelain` empty before and after all work)
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-b1-k3` (incremental on my ancestor configurations)
- Implementer handoff: `2e4cfc3:ops/team/messages/shell-bluetooth-applet/1788406407-annie-easley-composition-proof-repair-handoff.md`
- Prior verdicts read: K2.7 REJECT 0/1/0/2 (`lanes/review-b1-k27/verdict.md`) and my own ACCEPT 0/0/0/1 (`lanes/review-b1-k3/verdict.md`)

## Verdict

**ACCEPT.** P0=0, P1=0, P2=0, P3=0.

The K2.7 P1 is closed with a mutation-sensitive control, both P3s are closed
with wording that now matches the compiled contract field-for-field, and
nothing outside the three declared product paths (plus coordination files)
moved. This was a bounded repair recheck; the compiled surface test is
byte-identical to the candidate I already accepted, so I did not rerun the
hostile meta-object matrix.

## Findings ledger

### P0 — none

No destructive or host-affecting change. The new poison lane copies a handful
of product files into a caller-supplied `POISON_ROOT` and runs the same
script recursively with the poison section skipped; I ran it only under my
build root. No bus, hardware, uinput, network, or session rows touched.

### P1 — none

### P2 — none

### P3 — none

## Review questions

### 1. K2.7 P1 closed — reproduced symmetrically

I recreated K2.7's exact scratch edit in a pristine `git archive HEAD` copy at
`<ROOT>/scratch-composition`: removed the
`{"id": "bluetooth", "plugin": "bluetooth", ...}` line from
`data/profiles/qindaqt.json` and removed the
`import QindaQt.Shell.BluetoothApplet` line plus the
`BluetoothAppletModule.BluetoothApplet { id: bluetooth ... }` delegate from
`src/shell/qml/BuiltinAppletContent.qml`.

- Descendant gate:
  `cmake -DSOURCE_ROOT=<ROOT>/scratch-composition -P tests/shell/bluetooth_applet/check_runtime_boundary.cmake`
  → **exit 1**, diagnostics exactly:
  `missing production token 'QindaQt.Shell.BluetoothApplet'`,
  `missing production token 'BluetoothAppletModule.BluetoothApplet {'`,
  `missing production token '"plugin": "bluetooth"'`.
- Ancestor gate (from `git show 7061dd3:...`) on the identical edited tree →
  **exit 0**, "7 files and 0 poison rejections" — the regression K2.7 found is
  confirmed present at the ancestor and absent in the descendant.

Token inventory: diffing the descendant's `required_contracts` against base
`35f2fa2` shows all ten original presence tokens restored verbatim plus one
new delegate-construction token (`BluetoothAppletModule.BluetoothApplet {`)
— eleven total, matching the wiki's "eleven ... presence tokens".

Poison lane: the full-poison direct run prints **"7 files and 6 poison
rejections"** (5 dependency poisons + the new composition-removal poison), so
the sixth poison genuinely executes. The poison is not vacuous: it
`FATAL_ERROR`s if the profile entry or QML delegate cannot be removed from its
own copy, and after requiring a nonzero recursive result it additionally
requires the recursive output to contain all three exact missing-token
diagnostics — a generic nonzero cannot satisfy it. The recursion passes
`BLUETOOTH_RUNTIME_POLICY_SKIP_POISON=ON`, so it terminates.

### 2. Both P3s closed truthfully

- **Attribute wording (my P3-1, K2.7 P3-1):** `docs/wiki/shell/bluetooth-applet.md`
  now names the compared property fields: "name, type, readable, writable,
  resettable, notify signature, constant, and final ... plus every method's
  signature/return/type/access/revision and each enumerator". I checked this
  against the actual contract structs in
  `tests/shell/bluetooth_applet/tst_bluetooth_applet_surface.cpp:27-51`
  (`PropertyContract`: name/typeName/readable/writable/resettable/
  notifySignature/constant/final; `MethodContract`: signature/returnType/
  methodType/access/revision; `EnumeratorContract`: name/enumName/scope/flag/
  scoped/keysAndValues). The `testing-harness.md` wording ("property
  name/type/readable/writable/resettable/notify/constant/final fields and
  method signature/return/type/access/revision fields, plus each enumerator's
  name, enum name, scope, flag/scoped state, and key/value list") matches
  field-for-field. No residual "every attribute" overclaim.
- **"dispatcher" row name (K2.7 P3-2):** both Bluetooth sections now name the
  real `qindaqt.notification-center-applet-offscreen` row. The only remaining
  "dispatcher" in `testing-harness.md` (line 523) describes the notification
  center's audited QML entry-point dispatcher mechanism in a different
  section — a legitimate use, not the B1 adjacent-row list.

### 3. Nothing else moved

`git diff 7061dd3..882cc0c --stat` shows exactly the three declared product
paths (`tests/shell/bluetooth_applet/check_runtime_boundary.cmake`,
`docs/wiki/shell/bluetooth-applet.md`,
`docs/wiki/development/testing-harness.md`) plus the four coordination files
of intermediate parent `e251cf1` (three `ops/team/messages/` files and
`ops/team/workers/annie-easley.md`; `git show e251cf1 --stat` confirms it
touches nothing else). `git diff 7061dd3 882cc0c --
tests/shell/bluetooth_applet/tst_bluetooth_applet_surface.cpp` is empty — the
compiled surface gate is byte-unchanged, as claimed. No `src/` path changed;
no JSON changed.

### 4. Gates rerun

- `ctest -R '^qindaqt\.bluetooth-applet-' --output-on-failure --no-tests=error`:
  Debug 8/8, Release 8/8.
- Adjacent six-row selector
  `^(qindaqt\.(bluetooth-client|applet-manifest|applet-catalog|applet-runtime-resolution|notification-center-applet-offscreen|shell-runtime-catalog))$`:
  Debug 6/6, Release 6/6.
- Direct boundary modes against the worktree: no `POISON_ROOT` → exit 0, 7+0;
  `POISON_ROOT` + skip → exit 0, 7+0; full poison → exit 0, 7+6; pure
  boundary full poison → exit 0, 5+4.
- Static gates: `./tools/validate-docs` exit 0 (117 documents); pinned
  `mkdocs build --strict --site-dir <ROOT>/site` exit 0;
  `./tools/check-source-shape` exit 0 (only the pre-existing 500/539-line
  warnings); `git diff --check` and `git diff 7061dd3 882cc0c --check` exit 0;
  zero changed JSON files, per-changed-JSON gate not applicable.

## Commands and results (all mine, this review)

```text
git rev-parse HEAD                                 -> 882cc0cdbb31ee9d619c625a2856aee90c7a49b0
git rev-parse HEAD^{tree}                          -> 47611298c1a3dd6c11027d57ecfb214d206021c5
git merge-base --is-ancestor 7061dd3 882cc0c       -> exit 0
git status --porcelain (before and after)          -> empty
git diff 7061dd3..882cc0c --stat                   -> 3 product paths + 4 coordination files (e251cf1)

cmake --build <ROOT>/debug   (focused targets)     -> exit 0, no work to do (incremental; compiled surface unchanged)
cmake --build <ROOT>/release (focused targets)     -> exit 0, no work to do
ctest debug   '^qindaqt\.bluetooth-applet-'        -> exit 0, 8/8
ctest debug   adjacent six-row selector            -> exit 0, 6/6
ctest release '^qindaqt\.bluetooth-applet-'        -> exit 0, 8/8
ctest release adjacent six-row selector            -> exit 0, 6/6

descendant gate on K2.7-edit scratch tree          -> exit 1, three exact missing-token diagnostics
ancestor (7061dd3) gate on same scratch tree       -> exit 0 (regression reproduced symmetrically)
runtime boundary, no POISON_ROOT                   -> exit 0, 7 files + 0 poison rejections
runtime boundary, POISON_ROOT + skip               -> exit 0, 7 + 0
runtime boundary, full poison                      -> exit 0, 7 + 6
pure boundary, full poison                         -> exit 0, 5 + 4

./tools/validate-docs                              -> exit 0, 117 documents
mkdocs build --strict (pinned venv)                -> exit 0
./tools/check-source-shape                         -> exit 0, pre-existing warnings only
git diff --check / range --check                   -> exit 0
```

Scratch artifacts at `<ROOT>/scratch-composition` and
`<ROOT>/ancestor-gate.cmake`; poison roots under `<ROOT>/poison-*`. No product
path was edited.

## Nonclaims

I ran no `tests/session` nested-compositor rows, no host D-Bus services, no
hardware, uinput, or network. I did not rerun the ancestor's hostile
meta-object matrix because the compiled surface test is byte-identical to the
one I already validated at `7061dd3`. BluezQt/live-BlueZ behavior remains
outside this candidate's claims, as documented.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
