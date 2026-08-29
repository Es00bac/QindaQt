# D0 repair: exact virtual-backend gate and logical-size contract

- **Timestamp:** 2026-08-28T01:34:52Z
- **Worker:** Rhea Calder — Display D0 compositor-output lead
- **Audit consumed:** Dorian Vale handoff `1787880737-dorian-vale-display-d0-kwin-api-audit-handoff.md`

I accepted both blocking findings and repaired them in the D0 worktree.

- `src/compositor/kwin/qindaqtkwinplugin.cpp:38-69` now computes the exact three-part output gate once, uses it both to decide whether the KWin output seam exists and passes that distinct value to the output controller. The broad input/docking development gate no longer enables output request parsing on Wayland/DRM.
- `src/compositor/kwin/kwindevelopmentoutputseam.cpp:84-99` still rejects before name/number validation and before every backend query when that exact gate is false, preserving identical `control-disabled` responses for hostile and valid input.
- `src/compositor/kwin/kwindevelopmentoutputseam.h:39,54,99` and `.cpp:91-93,159,227-239` now name the requested width/height as logical size/dimensions, matching pinned VirtualBackend's `logical extent -> pixel mode size * scale` behavior.
- `src/session/sessionenvironment.cpp:23-36` clears inherited backend proof and sets exact `virtual` only for an explicit virtual test scenario; `src/compositor/kwin/mutationcontrol.cpp:22-39` requires that proof in addition to the existing two-part gate.

No compile or runtime command was run; Controls still owns that lane. I am continuing source/static work toward the D0 checkpoint.
