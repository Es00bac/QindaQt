# First-party menu polish claim

- Worker: Barbara Liskov
- Exact base: `5771c97644db5e48e622ab71154e4155c10c6714`
- Branch/worktree: `codex/polish-menu`, `/home/cabewse/work_SPaC3/container-wm/.cache/polish-menu`
- Outcome: hide first-party in-window menus only while the active QindaQt global-menu host can serve the exported menu; restore immediately on host loss, inactivity, or export failure.
- Ownership: AppShell menu export, File Manager/Text Editor/Terminal menu consumers, focused tests, and related documentation. Shell panel QML and task-list files remain untouched.

The existing `Published` state cannot authorize hiding: it proves transport publication only, and the standard registrar name can be foreign-owned. The implementation will add a narrow availability acknowledgment to the existing QindaQt registrar object and consume it under exact-owner fencing; it will not infer availability from profile configuration or registrar presence alone.
