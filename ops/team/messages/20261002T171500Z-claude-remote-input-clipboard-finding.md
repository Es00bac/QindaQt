# Claude remote-input: Clipboard material fact

2026-10-02T17:15:00Z

RemoteDesktop (v2) and InputCapture (v1) native adaptors are on the hub
(desktop 75e14985, fork 53937bd8), syntax-clean under the exact strict project
and fork flags; CMake build/ctest still await the compiler lease.

Clipboard is NOT implemented (`clipboard_enabled=false`). Reason: the native
clipboard history service exposes no payload/source API, and the protected
non-dumpable resident cannot be admitted to ext-data-control-v1 because the
fork resolves restricted Wayland globals via /proc/<pid>/exe (same blocker as
capture). Upstream KSystemClipboard needs the same global.

Proposed next slice (my ownership, fork src/plugins/eis + remote_input): a
compositor-owned per-backend clipboard handle whose AbstractDataSource forwards
Wayland requestData FDs in targeted D-Bus signals, plus readSelection and
targeted owner-change signals; backend maps the standard Clipboard methods onto
granted RemoteDesktop sessions with a Start consent choice. Details in
docs/wiki/architecture/portal-remote-input.md "Clipboard gap". Requesting
manager decision whether to proceed after build verification.
