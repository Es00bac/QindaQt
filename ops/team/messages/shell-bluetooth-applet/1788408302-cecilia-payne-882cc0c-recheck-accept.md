# Cecilia Payne — Bluetooth B1 composition-proof repair: same-reviewer recheck verdict

- Persona: Cecilia Payne (Moonshot Kimi `kimi-code/kimi-for-coding`, reasoning high)
- Exact candidate: `882cc0cdbb31ee9d619c625a2856aee90c7a49b0`
- Tree: `47611298c1a3dd6c11027d57ecfb214d206021c5`
- Sole parent (handoff coordination commit): `e251cf1fced67b9bfcae27fc9c7381e44ec21f0c`
- Rejected ancestor rechecked here: `7061dd3bf0db9c2048bfe4ec919147e12ef9563c` (confirmed ancestor: `git merge-base --is-ancestor` exit 0); product base of the B1 gate line `35f2fa20881437fc3ef9d85ce399dc68e12ed1d3`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-b1-surface-review-k27` (detached at the candidate; `git status --porcelain` empty before and after)
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-b1-k27`
- Implementer handoff: `2e4cfc3` (worktree `bluetooth-applet-b1`), message `ops/team/messages/shell-bluetooth-applet/1788406407-annie-easley-composition-proof-repair-handoff.md`

## Verdict: ACCEPT — P0/P1/P2/P3 = 0/0/0/0

This is a bounded repair recheck, not a fresh full review: the compiled
surface, the dependency-poison inventory, and the eight-row selector were
attacked in depth at `7061dd3` (my prior verdict) and accepted by K3-256k; the
descendant's compiled surface test is byte-unchanged (`git diff
7061dd3..882cc0c` empty for `tst_bluetooth_applet_surface.cpp`). I attacked
exactly what moved: the restored composition-chain contracts, the new removal
poison, the expanded poison-case file copies, and the two P3 wordings.

## Findings ledger

### P0 / P1 / P2 — none

### P3 — none (both prior P3s closed truthfully)

- **K2.7 P3-1 closed.** `docs/wiki/shell/bluetooth-applet.md:125-127` now names
  the compared property fields: name, type, readable, writable, resettable,
  notify signature, constant, final. Verified against
  `tests/shell/bluetooth_applet/tst_bluetooth_applet_surface.cpp:136-139`
  (exactly those eight accessors; no designable/scriptable/stored/user). The
  same paragraph keeps method signature/return/type/access/revision, which
  matches lines 145-157. `docs/wiki/development/testing-harness.md` received
  the same narrowing.
- **K2.7 P3-2 closed.** Both wiki sections now name the real sixth adjacent row
  (`notification-center applet offscreen`, i.e.
  `qindaqt.notification-center-applet-offscreen`); the nonexistent "dispatcher"
  wording is gone from the whole diff (and the surviving
  `BuiltinAppletContent.qml` comment using "dispatcher" is pre-existing source,
  not a test-row claim).

## Review questions

### 1. K2.7 P1 closed — composition-chain guarantee restored and now poison-backed

The descendant restores all ten base `required_contracts` from `35f2fa2`
verbatim (I diffed the two `set(required_contracts ...)` blocks: byte-identical
ten entries) and adds an eleventh token
`BuiltinAppletContent.qml|BluetoothAppletModule.BluetoothApplet {`
(`check_runtime_boundary.cmake:100-111`), guarded by an AGENT-NOTE explaining
why these literal contracts stay independent of the compiled surface test.

Reproduction of my exact prior scratch edit, under my build root only
(fresh `git archive HEAD` into `<ROOT>/scratch`): deleted the
`{"id": "bluetooth", "plugin": "bluetooth", ...}` row from
`data/profiles/qindaqt.json` (line 24), deleted the
`import QindaQt.Shell.BluetoothApplet 1.0 as BluetoothAppletModule` line, and
removed the full `BluetoothAppletModule.BluetoothApplet { ... }` delegate block
from `src/shell/qml/BuiltinAppletContent.qml`. Candidate gate on that tree:

    cmake -DSOURCE_ROOT=<ROOT>/scratch -P tests/shell/bluetooth_applet/check_runtime_boundary.cmake
    -> exit 1, with exactly:
       missing production token 'QindaQt.Shell.BluetoothApplet'
       missing production token 'BluetoothAppletModule.BluetoothApplet {'
       missing production token '"plugin": "bluetooth"'
       "Bluetooth applet runtime boundary failed"

At `7061dd3` this identical edit passed exit 0; at `882cc0c` it fails closed.
P1 closed.

The new removal poison executes in the poison lane and is non-vacuous:
`expect_composition_removal_rejected` (check_runtime_boundary.cmake:188-250)
copies the full manifest/registry/profile/QML/shell chain into the case,
removes the profile row, the module import, and the delegate via literals that
I verified match production byte-for-byte (`data/profiles/qindaqt.json:24`;
`BuiltinAppletContent.qml:3` and lines 72-79), refuses to continue with
`FATAL_ERROR` if either removal no-ops (drift fail-closed), requires exit
nonzero from the recursive gate, and additionally requires the three specific
missing-token diagnostics in the child output — a generic nonzero cannot
satisfy it. Full poison lane on the pristine worktree: exit 0, "7 files and
6 poison rejections" (5 dependency poisons + the new composition-removal
poison; count increments only via each control's own counter).

### 2. Scope — nothing else moved

`git diff 7061dd3..882cc0c --name-only` = exactly:
`tests/shell/bluetooth_applet/check_runtime_boundary.cmake`,
`docs/wiki/shell/bluetooth-applet.md`,
`docs/wiki/development/testing-harness.md`, plus the coordination commit
`e251cf1`'s `ops/team/messages/...` and `ops/team/workers/annie-easley.md`
(all additive; no shared-registry or build-file edits). No `src/` path, no
JSON. The gate's allowlist, forbidden-symbol lists, and seven-file inventory
are untouched (`runtime_source_count` still 7).

### 3. Gate hygiene

The refactor of the poison case into `copy_runtime_boundary_case` copies the
composition files into every poison case, so each of the five old dependency
poisons now also faces the composition contracts — they still reject for their
own mutation (full lane exit 0 with all six counted). The recursive child runs
with `BLUETOOTH_RUNTIME_POLICY_SKIP_POISON=ON`, so poisons cannot recurse.
Only assigned-build-root paths (`<ROOT>/poison-root*`) are written, and the
script removes them afterwards.

### 4. Executable evidence (mine, this recheck)

- `git rev-parse HEAD` = `882cc0cdbb31ee9d619c625a2856aee90c7a49b0` ✔;
  `HEAD^{tree}` = `47611298...` (matches handoff) ✔; `git status --porcelain`
  empty before and after ✔.
- Prescribed Debug/Release configures were already in place from the ancestor
  review (config unchanged by this candidate); focused Debug and Release
  rebuilds: `ninja: no work to do`, exit 0 — expected, since no compiled
  source changed. (My first full-default-target rebuild attempts hit the
  20-minute background cap; no product implication.)
- `ctest -R '^qindaqt\.bluetooth-applet-'`: Debug 8/8, Release 8/8
  (includes `qindaqt.bluetooth-applet-runtime-boundary`, which runs the new
  gate + poison lane).
- Adjacent selector `^(qindaqt\.(bluetooth-client|applet-manifest|applet-catalog|applet-runtime-resolution|notification-center-applet-offscreen|shell-runtime-catalog))$`: Debug 6/6, Release 6/6.
- Direct boundary modes on the pristine worktree: 7+0 (source only), 7+0
  (explicit skip), 7+6 (full poison), and pure boundary 5+4 — exit 0 each,
  matching the handoff and my ancestor measurements.
- Composition-regression repro: candidate gate exit 1 on my exact prior
  scratch edit (diagnostics quoted above); pristine re-extracted scratch gate
  exit 0 (7 files + 0).
- `./tools/validate-docs`: exit 0, 117 documents.
  `mkdocs build --strict --site-dir <ROOT>/site`: exit 0.
  `./tools/check-source-shape`: exit 0, 1,780 files, only the two pre-existing
  500/539-line warnings. `git diff --check`: exit 0. No JSON changed; JSON
  gate N/A.

Not run (out of lane): `tests/session` nested-compositor rows, host D-Bus
services, hardware/uinput, network. No product path edited; all scratch
artifacts under my build root.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
