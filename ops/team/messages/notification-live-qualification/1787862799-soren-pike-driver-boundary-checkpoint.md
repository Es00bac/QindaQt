# Notification live driver boundary and shared-registry checkpoint

- **Timestamp:** 2026-08-27T14:33:19-06:00
- **From:** Soren Pike
- **To:** Manager, compositor/session, shell, and documentation owners
- **State:** architecture checkpoint before source/shared-registry edits
- **Exact base:** `c4982697858c083828bd406f1aa56c4e942bcc10`

## Environment facts

The installed coherent KDE stack contains KWin, KScreenLocker, KGlobalAccelD,
LayerShellQt, and Qt Wayland at the versions required by the repository. KWin
links the real `libKScreenLocker` and `libKGlobalAccelD`; the latter's own
public header explicitly records that KWin constructs the daemon object. The
disposable private-bus test can therefore require all lock/shortcut names to be
owned by the exact nested KWin PID rather than starting helper impostors.

## Focused architecture

I will add one small Python process orchestrator plus focused C++ evidence
collaborators under `tests/session/**`. The orchestrator owns only disposable
XDG/bus/process lifecycle and staged-install discovery. C++ owns bounded D-Bus
protocol interaction, notification submission, Settings1 truth, Wayland trace
evidence, and test input. Production shell/session paths remain supervisor →
descriptor-token host/shell → exact compositor-PID lock authentication.

The existing development input device cannot express `N`, `Tab`, `Escape`, or
`Space`, so it cannot qualify the required production keyboard path. The
smallest necessary production change is to extend only its explicitly gated,
bounded key enum/codec/KWin mapping and its focused unit tests. Production still
rejects `InjectTestInput` before parsing when no validated scenario marker is
present; no host input or public automation route is added.

Live discovery also shows the current explicit header focus cycle traps Tab
inside four center controls and cannot traverse notification-card controls.
The smallest shell repair will establish a complete deterministic forward and
reverse chain across header, active/recent card actions, retry state, and
settings action, with focused offscreen coverage before nested evidence.

## Shared coordination points

Planned additive edits are limited to:

- `tests/session/CMakeLists.txt`: one focused parser/unit target and the staged
  notification matrix rows (1080p, WUXGA, 1440p, 125%, 150%);
- `src/compositor/CMakeLists.txt` / compositor test registry only as required by
  the expanded development-input enum tests;
- `docs/wiki/development/testing-harness.md`,
  `docs/wiki/shell/notification-presentation.md`, and any exact protocol
  reference whose supported-key list changes;
- `mkdocs.yml` only if a new page proves necessary (not currently planned).

No build is active from this lane. Per manager resource coordination, heavy
Debug/Release/sanitizer/nested work is deferred; when released it will run at
`--parallel 2` and a new board checkpoint will announce it.

