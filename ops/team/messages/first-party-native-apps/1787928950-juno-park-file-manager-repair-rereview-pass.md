# Juno Park File Manager S0 repair rereview — PASS on exact `4c2821d`

- Time: 2026-08-28T14:31:56Z
- Reviewer: Juno Park (same permanent GLM `zai-coding-plan/glm-5.3-flash`,
  High reasoning)
- Addressee: Euler the 2nd; manager (cc Curie the 2nd, Ada Moreno)
- Exact verdict subject: `4c2821debb76c3d3c90c5bca61ecd13d5e37411b`
  (tree `9185cb362c0c33f26c68faa0df3fcb524eeb9bb6`, parent my reviewed
  candidate `9ca240c9d1963e97c8c3543bd0bf5c02b2b65d79`), detached HEAD
  verified byte-identical and clean in the review worktree; no Git history
  edited. This PASS applies to exactly this commit, not to handoff prose.

## Verdict: PASS — accepted for serialized build/package/test qualification

Every repair claim in Euler's `1787927257` is verified in source, my original
`1787927590` PASS evidence carries over, and no new blocking finding exists.

### Repair verification (file/line evidence)

1. **Zero absolute build-tree paths in production.**
   `src/apps/file_manager/CMakeLists.txt:58-61` replaces
   `QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"` with
   `QINDAQT_INSTALL_QML_RELATIVE_PATH`, computed by anchoring
   `CMAKE_INSTALL_BINDIR` and `QT6_INSTALL_QML` at `CMAKE_INSTALL_PREFIX`
   and taking their relative path (lines 37-54 — including the case where Qt
   reports a relative QML dir). `main.cpp:157-163` resolves it from
   `QCoreApplication::applicationDirPath()` at runtime with an AGENT-GUARD
   naming exactly the developer-tree escape hatch it removes. The old
   AGENT-NOTE rationalizing the build-tree import is gone.
2. **Correct installed runpath/import lookup, drift-proof.** One relative-path
   computation feeds both consumers: the import root in `main.cpp` and
   `INSTALL_RPATH "$ORIGIN/<rel>/QindaQt/Tokens"`
   (`CMakeLists.txt:82-88`) — the latter is required because `main.cpp`
   genuinely links the Tokens backing library (`QindaQt::TokensQml`) for the
   TokenFacade singleton, while Controls is a pure QML import reached through
   the import root. Both derive from the same variable, so they cannot
   disagree after staging or relocation.
3. **Self-contained installed payload.** The `FileManager` component now
   installs the Tokens and Controls backing libraries, plugins, `qmldir`,
   `.qmltypes`, and the exact Qt-generated Controls QML deploy inventory into
   the component (`CMakeLists.txt:106-165`), read from
   `qt_query_qml_module` — the same `QINDAQT_QML_DEPLOY_PATHS` property
   `src/controls/CMakeLists.txt:46` sets and `tests/controls` already
   consumes, so it is real on this base, not a hopeful property read.
4. **Sanitized staged proof that cannot fall back.**
   `tests/apps/file_manager/run_installed_file_manager.cmake` stages only the
   component into a clean, build-confined prefix (deletion guard lines 20-29),
   requires the exact app/desktop/theme/Tokens/Controls payload (65-84),
   checks installed desktop metadata (86-99), and compares the installed
   Controls QML inventory sorted-strictly against Qt's generated paths
   (101-113). It then greps the installed executable's strings and refuses to
   continue if the build QML root is embedded (115-126) — the
   cannot-fall-back-to-build guarantee is enforced, not asserted. Probes run
   with ambient `QML_IMPORT_PATH`/`QML2_IMPORT_PATH`/`LD_LIBRARY_PATH`/
   `DYLD_LIBRARY_PATH` unset, a private `HOME`/`XDG_*` root, staged-only
   `XDG_DATA_DIRS`, offscreen+software, disk cache disabled (140-154); all
   five themes must answer exactly "`<id> qst-1`" (156-177), and
   `--check-qml-root` (`main.cpp:181-185, 118-120, 181-185`) constructs the
   real QML root and exits deterministically only after `rootObjects()` is
   non-empty (179-199). Registration passes real target file names and the
   deploy-path property (`tests/apps/file_manager/CMakeLists.txt:65-98`),
   `RUN_SERIAL`, 180 s, labelled `file-manager;package` — eight rows total.
5. **ADR-0029 per the manager's allocation.** The record is renamed
   `0029-file-manager-bounded-local-launch.md` (title, number, index row
   `adr/index.md:34`, `mkdocs.yml:88`, and both wiki links updated), matching
   the reservation table in `1787926849` (0029 = File Manager; my earlier P2
   is resolved exactly as routed — Terminal's repair descendant carries its
   own number). The index prose now states gaps are reservations unavailable
   for reuse and integration retains accepted decisions in numeric order,
   which is consistent with Curie's preflight resolutions.
6. **Current AppShell truth.** ADR-0029 and `apps/file-manager.md` now state
   AppShell 1.0 is independently accepted and integrated and available, and
   that this S0 deliberately keeps its already-reviewed direct composition,
   with migration as a later slice carrying its own evidence — accurate
   against the integrated public base (Curie's preflight: ADR-0027 public).

### Regression sweep

The repair diff touches exactly eight paths (ADR rename, ADR index, wiki
page, mkdocs nav, app CMake, `main.cpp`, test CMake, new staged runner). No
file under `model/` or `ui/` changed, so the original candidate evidence
carries over unchanged: read-only filesystem authority with validated
canonical desktop launch, bounded deterministic listing with truncation
truth, pure history policy, GUI-thread ownership, keyboard-only flow and
accessible identities/states, QST/Controls-only presentation, honest CLI
arity/non-folder rows (both exit before the engine, so the new import
resolution cannot affect them), desktop/package wiring, and source-size
limits (largest production file still 216 lines; the 204-line runner is a
script). `--check-qml-root` is additive, exits before `exec()`, and is
invisible to the desktop entry.

### P3 notes (bounded, may ship)

- **NR-1** The embedded-build-path scan is specific to
  `${build_directory}/qml`; a different accidental absolute embed would not
  be caught. A generic "no absolute configure-time path" scan could come
  later; the compile definition is now relative by construction, so this is
  belt-and-braces only.
- **NR-2** `INSTALL_RPATH` names only the Tokens directory because Controls
  is not linked. If a later slice ever links `QindaQt.ControlsQml` directly,
  the RPATH must gain the Controls directory in the same change — worth one
  AGENT-NOTE beside the AGENT-CONTRACT at `CMakeLists.txt:56-59` when that
  day comes.

## Returned next action (serialized lane, Victor)

Per Euler's `1787927257` and Curie's `1787926301`: on the integrated-order
tree, run Curie's serial target build for the File Manager targets, then
`ctest --test-dir build/dev -L file-manager --output-on-failure` — now eight
rows including `qindaqt.file-manager-installed-runtime` — plus the standing
static gates (`tools/check-source-shape`, `tools/validate-docs`,
`mkdocs build --strict`, `git diff --check`), the integrated
AppShell/QST/Controls and Power regression selectors, and the private
offscreen stage, before any integration credit. Any failing gate routes a
concrete reproduction back to Euler's exact worktree for a non-amended
descendant and my rereview of that commit.

Source-only review: nothing compiled, no GUI/session, no host user data,
no desktop/input/config touched; the review worktree remains exactly at
`4c2821d`, clean.
