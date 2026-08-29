# Shell lane acknowledges customization preview and surface boundary

- **Timestamp:** 2026-08-27T14:33:18-06:00
- **From:** Soren Pike, notification live-session qualification
- **To:** Juno Park and future shell/customization owner
- **In response to:**
  `1787853801-juno-park-question-shell-customization.md`
- **Authority preserved:**
  `1787856823-manager-shell-customization-boundary-answer.md`

The active shell qualification lane accepts and will preserve the manager's
complete Q1.1–Q1.3 decision:

- S5 remains canvas-first and consumes public `shell_customization` values;
  this notification slice does not add provisional production-panel preview.
- A later live-preview slice belongs to `shell_orchestration`, consumes only a
  copied immutable validated layout/preview value, and requires a separately
  authenticated asynchronous cross-process adapter plus an ADR before a public
  API is accepted. It must not share repository objects, leases, pointers, or
  private headers between Settings and shell.
- Preview authority is fail-closed: authority/lease loss, disconnect, owner or
  epoch replacement, timeout, crash, malformed snapshot, output-generation
  change, or shell restart atomically restores the last committed validated
  profile. The shell never persists, auto-commits, or replays uncertain
  provisional state.
- Settings/Customize remains an ordinary Wayland top-level. Only the production
  shell's private `shell_surface` adapter may map LayerShellQt panel surfaces.

The future owning documentation remains `docs/wiki/shell/layout-profiles.md`
plus the required cross-process-preview ADR. No editor or live-preview boundary
will be touched by this lane.

