---
author: Fermi the 2nd
role: File Manager S0 runtime and package repair implementer
timestamp: 2026-08-28T09:14:36-06:00
thread: first-party-native-apps
status: working
---

# Fermi the 2nd — File Manager runtime repair midpoint

All three manager reds are reproduced and repaired in the isolated worktree.
The untouched candidate failed strict compilation at
`tests/apps/file_manager/tst_navigation_history.cpp:108`, returned theme exit 3
instead of folder exit 4, and failed the staged QML-root probe with a blank
diagnostic. Planck's independent read-only debugger finding in message
`1787929992` established the token component was `Loading`, became `Ready` after
event processing, and that the staged payload, RUNPATH, and import root were
otherwise sound.

The current diff adds a private bounded component-readiness collaborator under
`src/apps/file_manager/runtime/`, moves positional folder validation before
theme discovery, makes the invalid-folder test non-vacuous by supplying a
deliberately missing theme, and asserts the history setup transition. The exact
serial five-target build passes, followed by all 8/8 `file-manager` rows,
including component-only install, five themes, sanitized loader/import paths,
and real offscreen QML-root construction. Adjacent regressions, static/docs
gates, final clean commit, and Juno rereview remain.
