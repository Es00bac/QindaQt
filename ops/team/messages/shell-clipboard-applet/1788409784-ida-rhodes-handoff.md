---
author: Ida Rhodes
date: 2026-09-02T22:29:44-06:00
topic: shell-clipboard-applet
type: handoff
candidate_commit: 759c639bc3978644447f78b3d223830581890d6c
candidate_tree: 1946d264f48691e7f9827bdb35153f480f6fc555
base_commit: 74da46345c7a5094d45c756ad8b23ca87591fcd3
branch: worker/clipboard-applet-c1
status: handoff
---

# Clipboard applet C1 handoff (salvage-and-finish)

## Candidate

- Exact candidate commit: `759c639bc3978644447f78b3d223830581890d6c`
- Tree: `1946d264f48691e7f9827bdb35153f480f6fc555`
- Exact base: `74da46345c7a5094d45c756ad8b23ca87591fcd3` (main at claim)
- Branch: `worker/clipboard-applet-c1` (worktree
  `/home/cabewse/work_SPaC3/container-wm-workers/clipboard-applet-c1`)

The branch tip is the candidate. Its history preserves Orion Vale's salvaged
chain verbatim (`edfca3e` cherry-pick of `5e48b5c`, `3b4a3b2` of `69b3edc`,
`1b21df4` of `610e81f`, all marked with `-x`), then `50a8951` (static
rework + repairs), `766d629` (fencing suite split + docs), `759c639` (QML
write-grant honesty + manifest count truth). Nothing was amended or rebased.
The salvage's edits to the read-only `src/services/clipboard_model/CMakeLists.txt`
and non-delegated `src/applets/CMakeLists.txt` were reverted in `50a8951`;
`git diff 74da463..759c639 -- src/services/clipboard_model src/applets/CMakeLists.txt`
is empty.

## What the candidate is

Compiled, registered, installable Clipboard applet over the integrated C0
model, mirroring the Power applet's pure/runtime STATIC split:

- `QindaQt::ShellClipboardApplet` (pure projection; ClipboardModel values +
  Qt Core only) and `QindaQt::ShellClipboardAppletRuntime` (injected
  `ClipboardClientInterface` seam, in-process `ClipboardModelClientAdapter`,
  shell-private `ClipboardAppletController`, compiled
  `QindaQt.Shell.ClipboardApplet 1.0` QML module).
- Least-authority gates: `clipboard.read` denial withholds all observation;
  `clipboard.write` denial keeps browsing/search but refuses every mutating
  intent in the controller AND disables the mutating controls in QML.
  Manifest `data/applets/clipboard.json` requests both capabilities;
  `data/applet-policy/default.json` grants them to the audited `clipboard`
  package only; `BuiltinAppletRegistry::firstParty()` admits
  `qindaqt.applets.clipboard`.
- Tarski P1 repairs carried/verified: real pointer events reach Pin/Delete
  (row MouseArea at `z: -1`, `qindaqt.clipboard-applet-qml-interactive-offscreen`
  uses real `mouseClick`), and search replies attribute by exact registered id
  + controller-internal query generation with a single deferred-drain path
  (hostile flush regression in `qindaqt.clipboard-applet-fencing`).
- New beyond the salvage: compiled-QML delegate `modelData` declaration
  (implicit reference is a ReferenceError in the compiled module), QST-1 role
  corrections (`outline.subtle`/`fg.subtle` do not exist), harness theme
  publication through a registered singleton factory (Qt 6.11 QuickTest has no
  `qmlEngineCreated` hook), capability gating end to end, registry/policy/
  resolver/catalog/manifest registrations, and a static-shape
  installed-package row with a relocated-consumer RPATH poison check.

## Evidence (all run by me on this tree)

Debug (`/home/cabewse/work_SPaC3/builds/qindaqt/clipboard-applet-c1/debug`,
strict warnings, exit 0 configure/build):

- `ctest -R '^qindaqt\.clipboard-applet-' --no-tests=error` → 10/10 pass
  (model, controller, fencing, seam, 4 offscreen QML rows, boundary-policy,
  installed-package).
- `ctest -R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime|applet-host)'`
  → 6/6 pass.
- `ctest -R '^qindaqt\.clipboard-model-'` (dependency-adjacent C0) → 4/4 pass.
- `ctest -R '^qindaqt\.(shell-runtime-catalog|shell-customization-applets)'`
  → 2/2 pass; `qindaqt-shell --list` prints `clipboard - Clipboard`.
- Mutation probe: swapping register/drain order in `dispatchSearch` makes
  `TstClipboardAppletFencing::testHostileSynchronousFlushCannotDisplaySupersededReply`
  FAIL (7/8) on the mutated tree; reverted and re-verified 8/8. The implicit
  delegate `modelData` reference was observed failing 3 interactive functions
  before the repair.

Release (`.../release`, strict warnings, exit 0 configure/build):

- `ctest -R '^qindaqt\.clipboard-applet-'` → 10/10 pass.
- `ctest -R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime|applet-host)'`
  → 6/6 pass.
- `ctest -R '^qindaqt\.clipboard-model-'` → 4/4 pass.
- `qindaqt-shell`/`shell-runtime-catalog` row: Debug only (see caveats).

Static gates (from the worktree root, candidate tree):

- `./tools/validate-docs` → exit 0 (117 documents).
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict
  --site-dir <ROOT>/site` → exit 0.
- `./tools/check-source-shape` → exit 0.
- `git diff --check` → exit 0.
- `python3 -m json.tool data/applet-policy/default.json` and
  `data/applets/clipboard.json` → exit 0.

## Changed paths (sorted, candidate vs base)

Product: `data/applet-policy/default.json`, `data/applets/clipboard.json`,
`docs/wiki/architecture/module-boundaries.md`,
`docs/wiki/development/testing-harness.md`, `docs/wiki/index.md`,
`docs/wiki/reference/applet-manifest-schema-v1.md`,
`docs/wiki/shell/applet-runtime.md`, `docs/wiki/shell/clipboard-applet.md`,
`mkdocs.yml`, `src/CMakeLists.txt`,
`src/applet_runtime/src/builtin_applet_registry.cpp`,
`src/shell/CMakeLists.txt`, `src/shell/clipboard_applet/**` (5 public headers,
5 sources, 2 QML files, CMakeLists), `tests/CMakeLists.txt`,
`tests/applet_runtime/tst_applet_instance_resolver.cpp`,
`tests/applets/tst_catalog.cpp`, `tests/applets/tst_manifest.cpp`,
`tests/shell/clipboard_applet/**` (CMakeLists, 4 C++ test sources, shared
fakes header, 4 QML test files, harness main, boundary script, installed
consumer project + probe, run script). Coordination: this thread and
`ops/team/workers/ida-rhodes.md`.

## Bounded caveats (deliberately not claimed)

- No production-shell dispatcher/runtime composition (`src/shell/runtime/**`,
  `src/shell/qml/**` are the composing lane's paths): the production panel
  does not render the applet yet, and the stock profile does not place it
  (that file is not in this lane's edit set).
- No live Wayland `ext-data-control-v1` transport, Clipboard1 bus surface,
  Settings1 opt-in wiring, or authenticated lock-state service; the adapter is
  the in-process C0 seam and lock is injected by composition/tests.
- `qindaqt.shell-runtime-catalog` and the `qindaqt-shell --list` observation
  were run in Debug only; Release evidence covers the focused and adjacent
  library-level selectors above.
- The installed-package row stages sibling artifacts (Controls/Tokens modules,
  C0 model/themes/design-tokens archives and headers, theme file) from the
  build/source tree because those install components belong to other owners;
  the RPATH poison check asserts the consumer references nothing outside the
  stage.
- If the platform lane later wants a *shared* applet library, the exact needed
  change outside my authority is `POSITION_INDEPENDENT_CODE ON` on
  `qindaqt_clipboard_model` (currently a non-PIC static archive). The static
  composition makes this unnecessary today.

## Requested next action

Independent exact review of `759c639bc3978644447f78b3d223830581890d6c`, then
manager integration.
