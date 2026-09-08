# ADR-0110: Own editor documents in ordinary windows

- **Status:** Accepted
- **Date:** 2026-09-08
- **Owners:** First-party applications / Text Editor
- **Supersedes:** The single-window collection hosting choice of ADR-0065 only

## Context

QindaQt containers provide grouping, tabs and splits for ordinary application
windows. An editor-owned tab strip duplicates that desktop responsibility and
hides individual documents from window-management operations. The existing
paths-only restore format must remain readable, and per-document atomic saves
must retain the guarantees in [ADR-0022](0022-keep-text-documents-local-and-atomic.md).

## Decision

`EditorApplication`, confined to the GUI thread, owns at most 32 ordinary
windows. Each `EditorWindow` owns exactly one controller, document view,
watcher, undo history, find bar and AppShell coordinator. New opens a window;
Open, CLI paths and drops use process-local canonical uniqueness and may reuse
a pristine untitled window. Save As rejects another window's canonical path.
The window manager alone provides tabs and splits.

Every shown window receives a separate menu export tied to its platform window.
Each export owns a distinct named session-bus connection: the shared exporter’s
fixed object path cannot be registered twice on one connection. Disconnect only
after withdrawing the export; other document windows retain their endpoints.
The export is destroyed before the coordinator it observes. Closing a window
runs that document's Save/Discard/Cancel consent and leaves other windows alive.
`file.close-window` / `fileCloseWindowAction` replace the retired tab-close IDs;
all `tabs.*` actions and `Ctrl+Tab` / numbered tab shortcuts are removed.
`Ctrl+Q` continues to close its current window.

The application owner is the sole restore writer. It retains the exact version
1 format, bounds, filesystem safeguards and default-off Settings1 policy from
[ADR-0065](0065-persist-text-editor-path-inventory.md). Paths reopen as windows
and `activeIndex` names the preferred active window. Explicit CLI paths take
precedence even when policy truth already exists at startup. Closing a nonfinal
window removes its path; the final close preserves its path for next launch.
No content, dirty state or editor history is persisted. The initial asynchronous
Settings1 acquisition does not clear a saved inventory before its first
confirmed snapshot; subsequent owner loss keeps the fail-closed clearing rule.

## Consequences

- Windows can be grouped, split or detached using ordinary desktop features.
- Closing the first created window does not end other document/menu lifetimes.
- Cross-process duplicate-path locking remains outside this contract; saves
  still use optimistic byte-revision checks.
- Tests cover independent windows/undo, canonical admission, drops, window-local
  consent and action routing, capacity, Save As collisions and restore precedence.
- Presentation consumes shared semantic tokens and the public icon catalog;
  no shell-private APIs or new compositor blur protocol are required.

The user-facing contract is in [Text Editor](../apps/text-editor.md).
