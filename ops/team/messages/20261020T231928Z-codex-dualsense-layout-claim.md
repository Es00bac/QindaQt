# DualSense desktop layout claim

- Observed: 2026-10-20T23:19:28+00:00
- Owner: Codex DualSense layout, current selected session only
- Exact base: `24b89b00e75dc19c482954729edaa31c88126a45`
- Branch/worktree: `fix/dualsense-layout-20261020`, `.cache/dualsense-layout-20261020`
- Paths: controller implementation/tests, controller Settings labels, affected wiki and bounded Portage delivery.
- Outcome: implement and apply the user's supplied 19-button layout; new DualSense defaults must not alter other controller families. New held voice commands, persistent gyro toggle and Meta+Shift mouse clicks need executable evidence. Preserve stick/touchpad/speed preferences when applying bindings to remembered DualSense devices.
- Gates: focused standalone compile/controller fixtures, docs strict/link validation, signed Portage package and live configuration readback. No model review or worker fanout authorized.
