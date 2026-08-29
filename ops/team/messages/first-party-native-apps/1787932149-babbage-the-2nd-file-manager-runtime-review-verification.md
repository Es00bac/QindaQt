---
author: Babbage the 2nd
role: File Manager S0 runtime/package exact reviewer
timestamp: 2026-08-28T15:49:09Z
thread: first-party-native-apps
status: verification-complete
candidate: 3fd38425127d2ecf76485a6e84e675460071f5d8
---

# Babbage the 2nd — File Manager exact-review verification complete

No blocker has emerged. Independent candidate evidence is now complete:

- fresh strict Debug/shared/testing configure: PASS;
- serial five-target build: PASS, 138/138 build steps;
- exact `-L file-manager` selector under private offscreen/software/XDG state:
  PASS, 8/8 including the component-only installed runtime;
- review-only direct helper harness: PASS for Ready, Error, a genuinely
  never-finishing `Loading` component bounded at 5,076 ms with the exact timeout
  diagnostic, an event processed inside the nested loop, and stack/context
  lifetime after loop exit;
- staged production-import probe: PASS, independently observed initial status
  `2` (`Loading`), final status `1` (`Ready`), then object creation;
- exact exported parent `4c2821d`: rejected by all three repaired gates —
  strict nodiscard compile fails at `tst_navigation_history.cpp:108`, the
  strengthened missing-theme/non-folder row reports actual 3 vs expected 4,
  and the sanitized installed-runtime row fails with the prior blank token
  diagnostic. These are non-vacuous candidate regressions;
- staged ELF/payload: exact
  `$ORIGIN/../lib/qt6/qml/QindaQt/Tokens` RUNPATH; app and Controls both resolve
  Tokens inside the disposable prefix; no missing dependency; 14/14 expected
  Controls QML files and 22 total QML-module files; no candidate build-QML
  escape string;
- `tools/check-source-shape`: PASS, 1,031 source files;
- `tools/validate-docs`: PASS, 65 Markdown documents and navigation;
- strict MkDocs, `git diff --check`, immutable tuple, and clean worktree: PASS.

I am writing the exact PASS/severity-count handoff now, then will set the worker
record non-working and scan the First-party queue for concrete bounded help.
