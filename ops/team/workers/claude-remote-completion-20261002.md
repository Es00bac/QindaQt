# Claude remote-completion worker

- Role: RemoteDesktop + ScreenCast contract completion (Claude Opus 5.5, Max effort); successor lane of claude-remote-input-20261002
- Status: working — source candidate d8b7ebb6e committed; waiting for a bounded -j2 desktop compiler slot (request 20261002T183154Z) to run focused private-bus tests once
- Base: desktop 32f2caf400637bd0263e30ea45aa56f79b8d5ada; fork 5f164f60012109c3caa93eb2b699db79ae7dbacd
- Worktrees: container-wm.worktrees/pf-claude-remote-completion-20261002; qindaqt-kwin.worktrees/pf-claude-remote-completion-20261002
- Branch: worker/pf-claude-remote-completion-20261002 (both repositories)
- Owned paths: src/services/portal/remote_input/, src/services/portal/capture/, src/services/portal/src/{screencast_adaptor,capture_sessions*,capture_policy,capture_wire,process_capture}.*, their public capture headers, minimal foundation composition seam, focused non-native tests, docs/wiki/architecture/portal-remote-input.md, affected capture reference/ADR pages; fork src/plugins/eis/ only if needed
- Not owned: tests/services/portal/remote_input/native/, shared fork native build, shortcuts, power, Settings permissions, packaging, icons

## Updates

- 2026-10-02T18:23:37Z: Claimed. Frontend 1.20.4 facts read from source: ScreenCast and RemoteDesktop impls are chosen independently (xdg-desktop-portal.c:385-393); ScreenCast.SelectSources on an RD session goes to the ScreenCast impl with the RD handle (screen-cast.c:536-607) and refuses persist/restore_token for RD sessions (screen-cast.c:447-458); RD Start results are read for streams/devices/clipboard_enabled/persist_mode/restore_data (remote-desktop.c:601-622); Notify* are forwarded fire-and-forget (NULL callback), so a NotSupported reply never reaches callers. Upstream KDE 6.6.6 shares one session registry (screencast.cpp:188-196). Source work only; no compiler slot requested yet.
- 2026-10-02T18:31:54Z: Production 59fd7b63c (ScreenCastSourceDelegate seam; RD Start publishes producer streams after input then capture consent; retire stops streams; producer/authority loss closes sessions; ScreenCast persist_mode/restore_data validated, never persisted), tests 6b6dd9479, docs d8b7ebb6e. Strict syntax-only clean. Requested -j2 slot for exact targets.
