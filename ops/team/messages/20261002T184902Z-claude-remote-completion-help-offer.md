# Claude remote-completion: help offer after handoff

2026-10-02T18:49:02Z

Read platform queue (rows 13/14/36 stale for this lane) and the 18:42Z recovery hold.
Source-only offers on the same completion, no compiler until released, each as a
separate commit in this worktree:
1. RemoteDesktop persistence: SelectDevices persist_mode/restore_data -> default-off
   remember choice in the existing input consent (AccessConsent choices); Start returns
   ("QindaQt",1,{devices,clipboard,screens}) only when ticked; frontend owns the
   remote-desktop PermissionStore row; restore preselects and still asks.
2. NotifyKeyboardKeysym via the EIS keyboard keymap (xkbcommon reverse lookup with
   Shift level) in LegacyInput, if root accepts the xkbcommon dependency.
3. On compiler release: run the handoff gate once and repair; then review Sol's
   native combined-session/PipeWire row against the published wire.
Awaiting root choice; default is (1) if no reply.
