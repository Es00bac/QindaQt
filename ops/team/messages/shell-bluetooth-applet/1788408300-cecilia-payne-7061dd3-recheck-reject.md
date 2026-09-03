# Cecilia Payne — Bluetooth B1 compiled surface-gate repair: same-reviewer recheck verdict

- Persona: Cecilia Payne (Moonshot Kimi `kimi-code/kimi-for-coding`, reasoning high)
- Exact candidate: `7061dd3bf0db9c2048bfe4ec919147e12ef9563c`
- Tree: `4ff0f7984cb1fc2bf4083fdb6e77a8b85028a0a8`
- Sole parent (base): `35f2fa20881437fc3ef9d85ce399dc68e12ed1d3`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-b1-surface-review-k27` (detached, product paths untouched; `git status --porcelain` empty before and after)
- Prior record: I REJECTED ancestor `af78bce` (P1 token-paste bypass); this is the same-reviewer recheck of the repaired descendant.
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-b1-k27`

## Verdict: REJECT — P0/P1/P2/P3 = 0/1/0/2

The compiled surface gate itself is sound: I verified nine hostile declaration forms against it in a scratch copy and every one fails closed. But the candidate also deletes the composition-chain positive checks (stock-profile placement, `BuiltinAppletContent.qml` wiring, shell composition tokens) that were not part of the PM's replace-the-regex-gate decision, and no remaining poison, row, or test covers them. The owning wiki page still claims those as B1 evidence.

## Findings ledger

### P1 — composition-chain guarantee silently dropped; documented B1 evidence no longer proven by any gate

The base gate (`35f2fa2:tests/shell/bluetooth_applet/check_runtime_boundary.cmake`, `required_contracts`) asserted ten production composition tokens, including `data/profiles/qindaqt.json|"plugin": "bluetooth"` (stock-profile placement), `src/shell/qml/BuiltinAppletContent.qml|QindaQt.Shell.BluetoothApplet` and `|qindaqt.applets.bluetooth` (shell QML wiring), `src/shell/runtime/bluetoothappletcomposition.cpp|BluetoothRead`/`|BluetoothControl`, and `src/shell/runtime/shellruntimeapplication.cpp|m_bluetoothApplet`. The candidate deletes all of them. These are simple `string(FIND)` presence checks — not the regex-exactness problem the PM decided was unfixable — so the replacement decision does not justify their removal.

Reproduction (all under my build root, review worktree untouched):

1. In `<ROOT>/scratch` (pristine `git archive HEAD` copy): removed the `{"id": "bluetooth", "plugin": "bluetooth", ...}` line from `data/profiles/qindaqt.json`, and removed the `import QindaQt.Shell.BluetoothApplet` line, the `bluetoothReady` property, the `BluetoothAppletModule.BluetoothApplet { id: bluetooth ... }` delegate, and the `bluetoothReady` branches from `src/shell/qml/BuiltinAppletContent.qml` (left `bluetoothAppletAccess` as a valid dangling property so the file stays valid QML).
2. Candidate gate on that edited tree: `cmake -DSOURCE_ROOT=<ROOT>/scratch -P tests/shell/bluetooth_applet/check_runtime_boundary.cmake` → **exit 0**, "passed (7 files and 0 poison rejections)".
3. Base gate on the identical edited tree: `cmake -DSOURCE_ROOT=<ROOT>/scratch -P <ROOT>/base-check_runtime_boundary.cmake` (from `git show 35f2fa2:...`) → **exit 1** with exactly: `missing production token 'QindaQt.Shell.BluetoothApplet'`, `missing production token 'qindaqt.applets.bluetooth'`, `missing production token '"plugin": "bluetooth"'`.

No remaining coverage: grep over `tests/` shows `BuiltinAppletContent`/`bluetoothappletcomposition`/`m_bluetoothApplet` appear in no test; `tests/profiles/` validates profile schema only (no stock-placement assertion); `qindaqt.shell-runtime-catalog` PASS_REGULAR_EXPRESSION checks only `notification-center - Notification Center`; the installed-package row greps the shell binary for the module URI (present via the linked plugin even with wiring removed) and only checks the staged `qindaqt.json` exists. The manifest/registry half of the old composition check remains well covered (`qindaqt.applet-catalog` capabilities equality; `qindaqt.applet-runtime-resolution` `registry.entryPoints()` equality including `qindaqt.applets.bluetooth`) — this finding is only about profile placement, shell QML wiring, and the shell composition chain.

Meanwhile `docs/wiki/shell/bluetooth-applet.md:12-16` (edited in this commit) still claims: "The B1 slice includes the ... manifest/registry/policy path, **stock-profile placement, production shell composition**, ...". After this commit a change removing bluetooth from the stock profile or unwiring the shell delegate passes the entire eight-row B1 selector and both boundary gates, so that documented claim exceeds what the code proves. Bounded repair: re-add the composition tokens as presence checks (they were not part of the failed regex mechanism) or move them into a compiled shell-composition test.

### P2 — none

### P3-1 — wiki says the surface test "compares every property attribute"; four QMetaProperty attributes are not compared

`docs/wiki/shell/bluetooth-applet.md` (new paragraph) and the commit message say the test compares "every property attribute". The test (`tests/shell/bluetooth_applet/tst_bluetooth_applet_surface.cpp`) compares name, typeName, readable, writable, resettable, notify, constant, final. `QMetaProperty::isDesignable/isScriptable/isStored/isUser` are not compared. Surface impact is nil for the QML threat model except `SCRIPTABLE false`, and that specific case is still caught because the property disappears from QML reflection and fails `qmlVisibleNamesMatchLiteralContract`. Precision fix: name the compared attributes in the wiki or add the four accessors.

### P3-2 — testing-harness names a nonexistent adjacent "dispatcher" row

`docs/wiki/development/testing-harness.md` says the B1 run "passed six adjacent public-client, manifest, catalog, resolver, **dispatcher**, and shell-runtime-catalog rows". No registered test row is named dispatcher; the actual sixth row in the handoff's (and my) adjacent selector is `qindaqt.notification-center-applet-offscreen` (`ctest -N | grep -i dispatch` returns nothing). Pre-existing wording carried through this edit; the count (six) is correct.

## Review questions

1. **Completeness/ordering of the meta-object comparison** — verified fail-closed for every hostile form I staged against the production controller header in `<ROOT>/scratch` (each built with the exact prescribed recipe and run with `QT_QPA_PLATFORM=offscreen`):
   - A. token-pasted `Q_PROPERTY` via two-level `##` macro in the controller header → test exit 1, `smuggledPaste|bool` reported first in the found slice;
   - B. backslash-newline-spliced `Q_PROPERTY` → exit 1, `smuggledSplice` reported;
   - C. comment-glued `Q_INVOKABLE/*glue*/` → exit 1, `smuggledInvokable()` reported;
   - D. `public Q_SLOTS:` member → exit 1, `smuggledSlot()` reported;
   - E. `Q_ENUM` on a new enum → exit 1, `enumerators expected [...]` mismatch naming `SmuggledEnum`;
   - F. swapped `diagnostic`/`summaryLabel` declaration order → exit 1 (ordered list comparison);
   - G. retyped `QString phase` → `QVariant phase` → exit 1, found `phase|QVariant` vs expected `phase|QString`;
   - H. **base-class change**: controller re-based onto a new `SmuggledBase : public QObject` with its own property — the meta-object contract slot alone still passes (offset slices see only controller-owned members), but `qmlVisibleNamesMatchLiteralContract` fails ("Compared lists have different sizes") because the inherited property is QML-visible and not subtracted by the plain-`QObject` baseline; the row fails overall. The pair of slots covers base-class changes even though the meta-object slot alone does not;
   - I. cross-header alias: `#define QINDAQT_ALIAS_PROPERTY Q_PROPERTY` in the allowed-include header `bluetooth_applet_types.h`, used from the controller header → exit 1, `smuggledAlias` reported (closes Sonnet's structural P3).
   Removed members trivially fail by list inequality. Not compared and out of QML-surface scope: `QMetaMethod` tag/name/parameter names, `QMetaProperty` designable/stored/user (see P3-1), and constructors (the controller is a context object, never a registered creatable QML type, so constructors are not QML-callable).
2. **Negative controls non-vacuous; QML check real and offscreen** — `comparisonRejectsExpandedSurface` first proves the empty contract matches an empty `QObject` surface (so the comparator is not always-mismatching), then requires a token-pasted property, public slot, signal, and enum on `ExpandedSurface` to be reported in every expanded category by the same `surfaceMismatch` function that gates production. The same comparator's production-side sensitivity is proven by hostile forms A–I above. The QML slot constructs the production `BluetoothApplet` through a real `QQmlComponent`/`QQmlEngine` with `import QindaQt.Shell.BluetoothApplet` (module URI, not a mocked object), reflects the controller exposed as a context value, subtracts a plain-`QObject` baseline, and QCOMPAREs names against the literal contract. It passes 1/1 in Debug and Release via ctest, and I reran the binary with `env -u DISPLAY -u WAYLAND_DISPLAY` → 5/5 passed, proving no display is required.
3. **Does removing the lexical gate weaken an uncovered guarantee?** — the controller-surface regex removal loses nothing (the compiled gate strictly dominates it, proven above). But the same commit also removed the composition-chain presence checks, which no remaining poison covers: the five kept poisons are service-include, address-accessor, persistence, filesystem, and standard-paths only. See the P1.
4. **Documentation truthfulness** — counts are truthful: eight registered `^qindaqt\.bluetooth-applet-` rows (presentation, request-state, controller, offscreen, surface, boundary, runtime-boundary, installed-package); I reproduced 7 files + 0, 7 + 0 (explicit skip), 7 + 5 (full poison), and pure 5 + 4 directly. New row descriptions match the code. Two precision defects: P3-1 (attribute overreach) and P3-2 (nonexistent "dispatcher" row), plus the P1 documentation overclaim about stock-profile placement and production shell composition.
5. **Earlier P3s** — Opus `3517ad3`'s four P3s were all escapes of the deleted lexical mechanism (token paste, splice+comment, trailing-whitespace splice, public `Q_SLOTS`); I re-verified token paste (A), splice (B), and public slot (D) fail the compiled gate, and trailing-whitespace splices reduce to "the compiler sees a member the test sees", so all are moot. Sonnet `921f638` P3-1 (mkdocs not run) is closed: `mkdocs build --strict` exit 0 here. Sonnet P3-2 (cross-header alias) is closed: form I fails closed. My own prior P1 (token paste) is closed by design.

## Commands and results (all mine, this review)

- `git rev-parse HEAD` = `7061dd3bf0...` ✔; `git status --porcelain` empty before and after ✔.
- Prescribed Debug configure: exit 0 (GCC strict). Prescribed Release configure: exit 0.
- Focused Debug build (applet libs + plugin + 5 bluetooth test executables + surface test + client lib, manifest/catalog/resolver tests, `qindaqt-shell`): exit 0, 379/379 actions. Identical Release build: exit 0, 383/383 actions.
- `ctest -R '^qindaqt\.bluetooth-applet-'`: Debug 8/8, Release 8/8 (isolated surface row 1/1 in both).
- Adjacent selector `^(qindaqt\.(bluetooth-client|applet-manifest|applet-catalog|applet-runtime-resolution|notification-center-applet-offscreen|shell-runtime-catalog))$`: Debug 6/6, Release 6/6 (after building the `qindaqt_bluetooth_client_tests` target my first pass missed; initial "Not Run" was my omission, not a product failure).
- Direct boundary modes: 7+0, 7+0 skip, 7+5 poison, pure 5+4 — all reproduced as the handoff claimed.
- `./tools/validate-docs`: exit 0, 117 documents. `mkdocs build --strict`: exit 0. `./tools/check-source-shape`: exit 0, 1,780 files, only the two pre-existing warnings. `git diff --check`: exit 0. No JSON changed in the candidate; JSON gate N/A.
- Hostile-form builds/runs A–I in `<ROOT>/scratch` + `<ROOT>/scratch-build`: as itemized above; every case failed closed with its own name in the mismatch.
- Composition-regression repro: candidate gate exit 0 vs base gate exit 1 on the identical edited scratch tree, violations quoted above.
- Not run (out of lane): `tests/session` nested-compositor rows, host D-Bus services, hardware/uinput, network. No product path was edited; all scratch artifacts live under my build root.
