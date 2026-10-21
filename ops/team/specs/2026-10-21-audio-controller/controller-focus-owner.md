# Lane: controller desktop input lost to passive gamepad holders (2026-10-21)

Branch `fix/controller-focus-owner-20261021` (worktree
`~/work_SPaC3/container-wm.worktrees/controller-focus-owner-20261021`), base hub
main `814f5e518`. Owner: Claude (Sonnet worker). No install or emerge until the package-once pass.

## Evidence (qinda-top, 2026-10-21 ~13:30)

A Bluetooth DualSense was connected and SDL-discovered (profile
`pad:f5723c95616eb32e232630bab632decc`, the correct serial-backed identity), but
Controllers1 reported `reason: "game"`. The only other holder of its nodes
(`/dev/hidraw4`, `/dev/input/event17`, `/dev/input/js0`) was `qindastream`
(QindaAnime, QtWebEngine), which has no gamepad code. Chromium, Firefox and
Electron open every gamepad once a page touches the Gamepad API, and keep it
open for the life of the process. `game_priority.cpp` with
`controller_runtime.cpp:231-232` treats any same-user holder as a game, so a
background web-engine app disables desktop control until it exits.

## New rule (ADR superseding the ADR-0347 holder clause)

Suppress as `game` only when a holder is not a passive web-engine host (maps
`libQt6WebEngineCore.so`, `libxul.so`, `libcef.so` or `icudtl.dat`) and is part
of the focused window's application. "Part of" means any of: same pid,
ancestor/descendant, same SID other than the compositor's SID, same cgroup that
is not `session-*.scope` or the compositor's, or Wine processes with the same
`WINEPREFIX`. All /proc reads stay on the ADR-0397 observer worker. Focus
changes invalidate only when a non-passive holder exists. Steam, lock/inactive
gating, the profile `enabled` toggle and neutral re-arm are unchanged.
