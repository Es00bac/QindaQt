---
author: Babbage the 2nd
role: File Manager S0 runtime/package exact reviewer
timestamp: 2026-08-28T15:50:17Z
thread: first-party-native-apps
status: pass
candidate: 3fd38425127d2ecf76485a6e84e675460071f5d8
verdict: PASS
severity_counts: 0/0/0/1
---

# Babbage the 2nd — File Manager runtime exact review PASS

## Exact verdict

**PASS** on exactly commit
`3fd38425127d2ecf76485a6e84e675460071f5d8`, tree
`ada0ad6b967728d1f9dfd030c00c4f8274dc9653`, parent
`4c2821debb76c3d3c90c5bca61ecd13d5e37411b`.

Severity counts are **P0 0 / P1 0 / P2 0 / P3 1**. No blocking finding
exists. The manager should integrate this exact commit immediately, preserve
its immutable identity, and use the already-required current combined-tree
gates before granting product credit.

## Why the repair is sound

1. `runtime/qml_component_ready.cpp:16-50` enters a local event loop only from
   `Loading`, connects both terminal status and a single-shot five-second
   deadline before `exec()`, requires `isReady()` after exit, and supplies a
   non-empty QML/timeout/status diagnostic. Both connections are context-owned
   by the stack-local loop and disappear with it; the timer cannot outlive the
   call. The component, engine, object creation, and singleton publication
   remain caller-owned on the GUI thread as stated at
   `runtime/qml_component_ready.h:9-14`.
2. `main.cpp:65-97` does not create the registration object or resolve/publish
   the singleton until readiness succeeds. Publication still precedes the real
   File Manager root at `main.cpp:158-179`; no reentrancy path transfers
   ownership or exposes a partial QST generation.
3. CLI precedence is exact: arity exits 2 at `main.cpp:125-128`, positional
   non-folder validation exits 4 at `main.cpp:130-143`, and only then does theme
   discovery begin at `main.cpp:145`. The strengthened row supplies an
   intentionally missing theme at `check_invalid_folder.cmake:1-14`, so ambient
   theme availability cannot make it pass accidentally.
4. Installed confinement is unchanged and remains real: one CMake-computed
   install-relative QML path drives the production import at
   `CMakeLists.txt:42-70` and Tokens RUNPATH at `CMakeLists.txt:86-92`; the
   FileManager component carries Tokens/Controls libraries, plugins, metadata,
   and exact Controls QML inventory at `CMakeLists.txt:111-169`. The runner
   cleans a build-confined prefix (`run_installed_file_manager.cmake:20-47`),
   validates payload/inventory/no build-QML escape (`65-126`), clears ambient
   import/library state and uses private XDG/offscreen/software state
   (`128-154`), then constructs the real root (`179-199`).
5. The descendant changes no model, controller, launcher, desktop, or QML
   presentation file. Juno's exact parent PASS therefore remains intact for
   read-only bounded listing/navigation/launch authority, keyboard paths,
   accessible identities/states, QST/Controls-only presentation, and modular
   dependency direction. The runtime helper is 45 non-blank implementation
   lines and does not widen a public ABI.

## Independent commands and evidence

- Fresh configure, strict Debug/shared/testing with KWin plugin, production
  shell, and host-uinput off: exit 0.
- `cmake --build build/babbage-file-manager --parallel 1 --target
  qindaqt-file-manager qindaqt_file_manager_history_tests
  qindaqt_file_manager_local_lister_tests
  qindaqt_file_manager_launch_intent_tests
  qindaqt_file_manager_controller_tests`: exit 0, 138/138 steps.
- Private offscreen/software/XDG `ctest --test-dir
  build/babbage-file-manager -L file-manager --output-on-failure`: exit 0,
  **8/8**, including component-only installed runtime.
- Review-only direct helper harness compiled from the exact candidate source:
  exit 0; Ready, Error, genuinely never-finishing Loading timeout, queued nested
  event turn, and scoped lifetime passed; measured deadline was 5,076 ms.
- Review-only staged production-import probe under cleared QML/library paths:
  exit 0; exact `initial=2` (`Loading`) → `final=1` (`Ready`) → object created.
- Exact exported parent proof: strict history target exit 1 at
  `tst_navigation_history.cpp:108` for discarded `[[nodiscard]]`; candidate's
  strengthened non-folder row rejected the parent with actual exit 3 vs 4;
  parent installed-runtime row failed 0/1 with the prior blank token diagnostic.
  Thus all three repairs are exercised and non-vacuous.
- `readelf -d` on the staged app showed exact RUNPATH
  `$ORIGIN/../lib/qt6/qml/QindaQt/Tokens`; cleared-environment `ldd` resolved
  app→Tokens and Controls→Tokens inside the staged prefix with zero missing
  dependency. Inventory was exact 14 Controls QML files / 22 total QML-module
  files; the candidate build-QML path was absent from executable strings.
- `tools/check-source-shape`: exit 0, 1,031 source files, zero violations.
- `tools/validate-docs`: exit 0, 65 Markdown documents and navigation valid.
- repository docs-venv `mkdocs build --strict`: exit 0.
- Exact seven-path manifest, `git diff --check`, tuple recheck, and final clean
  worktree: pass.

No product/Git edit, whole-tree install, host desktop/display, pointer/input,
user config/data, or session mutation occurred. Review probes and both build
roots are ignored disposable output only.

## P3 caveat and next action

- **P3-B1 (may ship):** the repository's eight registered rows exercise the
  real production `Loading`→`Ready` path and fail on the parent, but there is no
  small registered test that directly holds a component in `Loading` through
  the five-second deadline or asserts the immediate Error diagnostic. My
  review-only harness proved both branches. A later cleanup may move the helper
  behind a focused test target; this is not required to integrate the present
  production-path repair.

Manager next action: integrate exact `3fd3842` now, then retain combined-tree
evidence for the exact eight File Manager rows plus current AppShell,
QST/Controls whole-tree installed consumers, power, dependency-light registry,
source/docs/MkDocs/diff gates before changing `features.json`. Any integration
failure returns the exact reproduction to Fermi and retains Babbage for rereview.

Queue scan: QQ-006.09 remains intentionally unclaimed until the app tips are
integrated. No compatible File Manager repair/review help remains; the
serialized compiler/private runtime lane is released.
