# Chien-Shiung Wu — same-reviewer recheck verdict: Bluetooth B1 compiled surface-gate repair

- Timestamp: 2026-09-02
- Persona: Chien-Shiung Wu
- Provider/model: Moonshot Kimi `kimi-code/k3-256k`, reasoning high
- Exact candidate: `7061dd3bf0db9c2048bfe4ec919147e12ef9563c`
- Exact tree: `4ff0f7984cb1fc2bf4083fdb6e77a8b85028a0a8`
- Sole parent (= exact base): `35f2fa20881437fc3ef9d85ce399dc68e12ed1d3`
- Rejected ancestor I previously reviewed: `af78bce23c4f57d8085d9cd6b27f8b4eeecb26bb` (confirmed ancestor via `git merge-base --is-ancestor`)
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-b1-surface-review-k3` (detached at the candidate; `git status --porcelain` empty before and after all work)
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-b1-k3`
- Implementer handoff: `e251cf1:ops/team/messages/shell-bluetooth-applet/1788404319-annie-easley-metaobject-repair-handoff.md` (SHA/tree/parent/path inventory all match the immutable objects)

## Verdict

**ACCEPT.** P0=0, P1=0, P2=0, P3=1.

My prior P2 (splice+comment `Q_PROPERTY` escape accepted by the lexical gate) and
P3 (raw-count fence counting macro names inside comments) are both closed: the
entire lexical positive-surface mechanism is deleted, and the exact escape form
I reproduced against `af78bce` now fails the compiled surface test (exit 2, see
matrix below). All four Opus P3s and both Sonnet P3s are closed (details in
question 5).

## Findings ledger

### P0 — none

No destructive or host-affecting behavior. The new test links the fake
transport, runs under `QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software`,
and touches no bus, hardware, uinput, or network. I ran no session/nested rows.

### P1 — none

The outcome works as claimed in both profiles (8/8 Bluetooth rows, 6/6 adjacent
rows, Debug and Release); the comparison is two-sided, ordered, and sensitive
to every hostile form I staged (13 cases, all rejected); the in-test negative
control asserts per-category detection of a token-pasted property, a public
slot, a signal, and an enum, so it is not vacuous; documentation counts match
reality.

### P2 — none

No missing negative control and no weakened dependency-policy guarantee found
(see questions 2 and 3).

### P3 — one (nonblocking precision)

**P3-1: Wiki overclaims property-attribute coverage.**
`docs/wiki/shell/bluetooth-applet.md:126` says the surface test "compares
every property attribute", and `docs/wiki/development/testing-harness.md`
similarly says "complete attribute/signature lists". The property contract
(`tests/shell/bluetooth_applet/tst_bluetooth_applet_surface.cpp:28-37`)
compares name, type, readable, writable, resettable, notify signature,
constant, and final — but not `REVISION`, `DESIGNABLE`, `STORED`, `USER`,
`BINDABLE`, or MEMBER-vs-READ backing (methods do compare revision; properties
do not). Reproduction: adding `REVISION(1)` or changing MEMBER/READ backing on
an existing property changes no compared field, so both the meta-object walk
and (for these axes) the QML name check stay green. Impact is bounded: none of
these axes expands QML-callable authority, and a `SCRIPTABLE false` change
*would* be caught by the QML-visible name equality. Recommend narrowing the
wiki sentence to name the compared attributes. Nonblocking.

## Review questions

### 1. Completeness of the meta-object comparison — proven by construction and execution

The comparison walks `propertyOffset()`/`methodOffset()`/`enumeratorOffset()`
to the respective counts of
`BluetoothAppletController::staticMetaObject` and compares ordered, fully
described entries (`surfaceMismatch`,
`tst_bluetooth_applet_surface.cpp:202-244`). Because the authority is the
post-moc meta-object, source spelling (token pasting, splices, comments,
aliases) is irrelevant: anything moc registers is compared; anything moc
cannot see cannot become surface. I proved this empirically by copying the
worktree to `<ROOT>/scratch` (pristine header/cpp snapshotted and restored
after every case), configuring `<ROOT>/scratch-debug` with the exact
prescribed Debug recipe, and rebuilding only
`qindaqt_bluetooth_applet_surface_tests` per mutation:

| Hostile form staged in the scratch controller header | Result |
| --- | --- |
| Baseline (unmutated scratch) | exit 0, 5/5 pass |
| Plain `Q_PROPERTY` addition (control) | exit 2 — meta-object properties mismatch + QML name mismatch |
| Token-pasted `JOIN(Q_, PROPERTY)(...)` (K2.7 reject shape) | exit 2 — same |
| In-identifier splice + comment gap `Q_PROPERT\⏎Y/**/(...)` (my exact prior P2 form) | exit 2 — same |
| `public Q_SLOTS: void beginPairing(...)` (Opus P3.4 shape) | exit 2 — methods mismatch showing `beginPairing(QString)\|type=Slot` |
| Plain `Q_INVOKABLE` addition | exit 2 — methods mismatch |
| `Q_SIGNALS:` addition | exit 2 — methods mismatch (signal ordering also visible) |
| `enum class` + `Q_ENUM` addition | exit 1 — enumerators mismatch with keys/values |
| Property reorder (swap two lines) | exit 1 — ordered comparison fails |
| Property removal | exit 2 |
| Property retype `quint64`→`qint64` | build fails: `-Werror=sign-conversion` in moc output — fail-closed at compile time |
| Property retype `bool`→`int` (compiles clean) | exit 1 — meta-object mismatch `operationPending\|int` vs `\|bool` |
| Same-file `#define ALIAS Q_PROPERTY` used as the macro (Sonnet P3.2 analog) | exit 2 — moc expands the alias, property registered, caught |
| Injected intermediate base class carrying `Q_PROPERTY(bool smuggled ...)` between `QObject` and the controller | exit 1 — the offset walk passes *by design* (base slice excluded), and the QML-visible name check fails, so the gate still rejects |

Every added, removed, reordered, or retyped property/signal/slot/invokable/
enum failed the gate. The base-class case demonstrates the one structural
subtlety: `propertyOffset()` deliberately excludes inherited members, and the
offscreen QML reflection check (which subtracts only a plain `QObject`
baseline) is what closes that hole. Both layers are needed and both are
present.

### 2. Negative controls non-vacuous; QML check real and offscreen

- The in-test negative control `comparisonRejectsExpandedSurface` requires the
  expanded surface (token-pasted property, public slot, signal, enum) to be
  reported in *every* category by name; it passes, so the shared comparison
  function genuinely detects additions.
- Empirically, the two forms that the rejected lexical gate at `af78bce`
  *accepted* (token paste per K2.7's `b149ec2`; splice+comment per my
  `30d77ae`) now fail with exit 2 (matrix above) — the controls detect exactly
  the escapes that motivated the repair.
- `qmlVisibleNamesMatchLiteralContract` builds a real `QQmlEngine`, adds the
  build-tree QML import path, statically imports
  `QindaQt_Shell_BluetoothAppletPlugin`, instantiates the production
  `BluetoothApplet` component with a live controller bound as `access`, and
  reflects it in `Component.onCompleted`. It ran green in Debug and Release
  ctest under `QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software` with no
  display, and I reran the binary directly (exit 0, 5/5 pass).

### 3. Dependency-policy guarantees intact

The include allowlist, the seven-file inventory, and the forbidden-symbol
lists in `check_runtime_boundary.cmake` are byte-identical in effect; the five
remaining poisons (service include, address accessor, persistence, filesystem,
standard paths) each still execute and reject — I reproduced the full poison
run: `7 files and 5 poison rejections`, exit 0. The six removed surface
poisons are superseded by a strictly stronger compiled check. The removed
`required_contracts` token-presence audit (manifest id/capabilities, registry
token, composition grant tokens, profile plugin entry) is superseded
behaviorally by rows I reran: `qindaqt.applet-catalog` asserts the real
manifest's `{BluetoothRead, BluetoothControl}` capabilities,
`qindaqt.applet-runtime-resolution` resolves the real profile/policy/registry
chain and asserts the granted `bluetooth.read`/`bluetooth.control` set, and
`qindaqt.bluetooth-applet-installed-package` requires the relocated shell to
list `bluetooth - Bluetooth` under source-path poison. The old audit was
presence-only (it would not have caught a read/control swap); nothing it
actually guaranteed is lost. No remaining guarantee depends on the deleted
regexes.

### 4. Documentation truthfulness

Verified counts: 8 Bluetooth rows (listed, all pass in both profiles), 6
adjacent rows (pass), 5 runtime poisons, 4 pure poisons, 117 validated
documents, 1,780 source files with only the two pre-existing decomposition
warnings, new test 364 non-blank lines (handoff claims 364), no JSON changed
(per-file JSON gate correctly not applicable). Gate descriptions in
`docs/wiki/shell/bluetooth-applet.md:181-184` and
`docs/wiki/development/testing-harness.md` match what the code does,
including truthful nonclaims. One overclaim: "every property attribute" (P3-1
above). No stale lexical-gate claims remain anywhere under `docs/`.

### 5. Earlier P3s closed or carried

- Opus `3517ad3` #1 (splice+comment count-fence bypass): **closed** — mechanism
  deleted; the exact form now fails the compiled test (exit 2).
- Opus #2 (token pasting): **closed** — compiled gate; proven exit 2; in-test
  negative control uses token pasting.
- Opus #3 (trailing-whitespace splice): **closed** — source spelling is
  irrelevant to a meta-object gate; GCC `-Werror` also rejects it at compile
  time in this project.
- Opus #4 (public `Q_SLOTS` invisible to macro gate): **closed** — slots appear
  in the ordered method comparison as `type=Slot`; proven exit 2.
- Sonnet `921f638` #1 (`mkdocs build --strict` unreproduced): **closed** — I
  ran it: `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build
  --strict --site-dir <ROOT>/site`, exit 0.
- Sonnet #2 (cross-header macro alias): **closed** — the compiled gate is
  spelling-independent; a same-file alias is proven caught (exit 2), and an
  alias moc cannot expand cannot create meta-object surface at all.
- My `30d77ae` P2 (splice+comment accepted): **closed** (exit 2 above).
- My `30d77ae` P3 (raw-count fence also counts macro names in comments —
  fail-closed brittleness): **closed** — the fence is deleted.

## Commands and results (all run by me against the exact candidate)

```text
git rev-parse HEAD                                   -> 7061dd3bf0db9c2048bfe4ec919147e12ef9563c
git rev-parse HEAD^{tree}                            -> 4ff0f7984cb1fc2bf4083fdb6e77a8b85028a0a8
git rev-parse HEAD^                                  -> 35f2fa20881437fc3ef9d85ce399dc68e12ed1d3
git merge-base --is-ancestor af78bce... HEAD         -> exit 0
git status --porcelain (before and after all work)   -> empty
git diff --check ; git diff HEAD^ HEAD --check       -> exit 0
git diff HEAD^ HEAD --name-only -- '*.json'          -> 0 files (JSON gate N/A)

cmake -S . -B <ROOT>/debug   (exact prescribed recipe) -> exit 0
cmake --build <ROOT>/debug --parallel 3 --target
  qindaqt_shell_bluetooth_applet qindaqt_shell_bluetooth_applet_runtime
  qindaqt_shell_bluetooth_applet_runtimeplugin
  qindaqt_bluetooth_applet_{presentation,request,controller,qml,surface}_tests
  qindaqt_bluetooth_client qindaqt_bluetooth_client_tests
  qindaqt_applet_manifest_tests qindaqt_applet_catalog_tests
  qindaqt_applet_instance_resolver_tests qindaqt-shell      -> exit 0, 383/383 actions
ctest --test-dir <ROOT>/debug -R '^qindaqt\.bluetooth-applet-'
  --output-on-failure --no-tests=error                     -> exit 0, 8/8
ctest --test-dir <ROOT>/debug -R '^(qindaqt\.(bluetooth-client|applet-manifest|
  applet-catalog|applet-runtime-resolution|notification-center-applet-offscreen|
  shell-runtime-catalog))$' --output-on-failure --no-tests=error -> exit 0, 6/6

cmake -S . -B <ROOT>/release (exact prescribed recipe) -> exit 0
identical focused Release build                          -> exit 0, 383/383 actions
identical Release Bluetooth selector                     -> exit 0, 8/8
identical Release adjacent selector                      -> exit 0, 6/6

cmake -DSOURCE_ROOT=$PWD -P tests/shell/bluetooth_applet/check_runtime_boundary.cmake
  -> exit 0, "7 files and 0 poison rejections"
  + -DPOISON_ROOT=<ROOT>/poison-root -DBLUETOOTH_RUNTIME_POLICY_SKIP_POISON=ON
  -> exit 0, "7 files and 0 poison rejections"
  + -DPOISON_ROOT=<ROOT>/poison-root (full poison lane)
  -> exit 0, "7 files and 5 poison rejections"
cmake -DSOURCE_ROOT=$PWD -DPOISON_ROOT=<ROOT>/poison-root-pure
  -P tests/shell/bluetooth_applet/check_boundary.cmake
  -> exit 0, "5 files and 4 poison rejections"

./tools/validate-docs          -> exit 0, 117 Markdown documents + navigation
mkdocs build --strict (pinned venv, --site-dir <ROOT>/site) -> exit 0
./tools/check-source-shape     -> exit 0, 1,780 files, only the two pre-existing
                                  500/539-line warnings; new test 364 non-blank lines
```

Hostile matrix: driver and logs at `<ROOT>/run-hostile-matrix.sh` and
`<ROOT>/hostile-matrix.log` (+ per-case `hostile-build-*.log` /
`hostile-run-*.log`); scratch tree at `<ROOT>/scratch`, restored pristine
afterwards (final scratch rebuild green: exit 0, 5/5 pass).

## Nonclaims

I ran no `tests/session` nested-compositor rows, no host D-Bus services, no
hardware, uinput, or network. The surface test's authority is the moc
meta-object and offscreen QML reflection; I did not attempt forms requiring a
malicious edit to the test-policy scripts themselves. BluezQt/live-BlueZ
behavior remains outside this candidate's claims, as documented.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/1
