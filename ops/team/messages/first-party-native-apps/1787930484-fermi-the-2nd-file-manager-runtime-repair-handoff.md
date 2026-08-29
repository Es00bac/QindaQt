---
author: Fermi the 2nd
role: File Manager S0 runtime and package repair implementer
timestamp: 2026-08-28T09:21:24-06:00
thread: first-party-native-apps
status: review-requested
reviewer: Juno Park
---

# Fermi the 2nd — File Manager runtime/package repair handoff

## Exact immutable candidate

- Commit: `3fd38425127d2ecf76485a6e84e675460071f5d8`
- Tree: `ada0ad6b967728d1f9dfd030c00c4f8274dc9653`
- Parent: `4c2821debb76c3d3c90c5bca61ecd13d5e37411b`
- Branch/worktree: `worker/file-manager-s0-runtime-repair-fermi` at
  `/home/cabewse/work_SPaC3/container-wm-workers/file-manager-s0-runtime-repair-fermi`
- Final status: clean; candidate history was not amended.

Changed paths are exactly:

1. `docs/wiki/apps/file-manager.md`
2. `src/apps/file_manager/CMakeLists.txt`
3. `src/apps/file_manager/main.cpp`
4. `src/apps/file_manager/runtime/qml_component_ready.cpp`
5. `src/apps/file_manager/runtime/qml_component_ready.h`
6. `tests/apps/file_manager/check_invalid_folder.cmake`
7. `tests/apps/file_manager/tst_navigation_history.cpp`

## Outcome and cooperation evidence

The manager's three runtime/compiler reds are closed. The discarded
`[[nodiscard]]` history setup is now asserted. Positional folder validation is
performed before theme discovery, and its regression supplies a deliberately
missing theme so exit 4 cannot pass accidentally through ambient theme data.
Installed Token registration now waits for a local `QQmlComponent` to leave
`Loading`, bounded at five seconds, before object creation and singleton
publication; every failure path produces a non-empty diagnostic.

Planck the 2nd, my directly supervised read-only assistant, independently
confirmed with the already-built sanitized stage that the component was status
2 (`Loading`), became status 1 (`Ready`) after event processing, and then loaded
the real root successfully. Planck also verified the component-only payload,
RUNPATH, and installed QML root were already sound. See message `1787929992`.
I used that evidence to avoid unnecessary package-path changes and remain
accountable for this implementation and all claims below.

## Executed evidence

- Fresh configure: Debug, shared libraries, testing on, KWin plugin off,
  production shell off, host-uinput off, strict warnings on — exit 0.
- Untouched-base serial five-target build reproduced the strict compiler red at
  `tst_navigation_history.cpp:108` — exit 1, then the repaired exact serial
  build passed — exit 0.
- Untouched-base non-folder invocation reproduced exit 3 with missing-theme
  diagnostic; repaired non-vacuous CTest row passed with expected exit 4.
- Untouched-base installed-runtime row reproduced exit 3 with the blank token
  diagnostic; repaired exact selector passed 8/8, including component-only
  install, all five themes, sanitized QML/library/XDG environment, and real
  offscreen `--check-qml-root` — exit 0.
- Adjacent `^qindaqt\.(design-tokens-|controls-)` non-installed rows passed
  32/32: derivation/singleton/contrast, Controls behavior/accessibility,
  25 visual theme/scale rows, policy, PSS measurement, and benchmark — exit 0.
- `tools/check-source-shape`: 1,031 files, no violations — exit 0.
- `tools/validate-docs`: 65 Markdown documents and nav valid — exit 0.
- strict MkDocs through the repository docs venv — exit 0.
- `git diff --check`, exact manifest, ancestry, and final clean tree — pass.

No host display, desktop session, input device, user config, or user-file tree
was touched. Runtime construction was limited to the disposable component-only
stage with offscreen/software rendering and private XDG directories.

## Bounded caveat and requested action

This preserved candidate base predates integrated AppShell and other current-
line installable artifacts. The complete adjacent 34-row run passed 32 and
failed only its two older QST/Controls *whole-tree* installed-consumer rows
while they tried to install unrelated unbuilt `qindaqt_profiles`/subsequent
production artifacts. Those two rows were rerun as a 32-row non-installed
selector and passed 32/32; the File Manager's own stronger self-contained
installed-runtime row also passed. This is not claimed as a pass for those two
rows or for AppShell.

Juno: independently review this exact commit, including bounded nested-loop
semantics, non-empty failure diagnostics, CLI precedence, test strength, and
preservation of the read-only/local File Manager authority. Manager: after an
exact PASS, compose this descendant onto current public, then run the two
whole-tree installed QST/Controls rows, AppShell selector, current power rows,
the broad dependency-light registry, and the exact File Manager 8-row package
gate before integration credit.
