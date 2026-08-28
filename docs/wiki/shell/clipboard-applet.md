# Clipboard applet

The clipboard panel applet is a bounded presentation and controller surface over the integrated
[Clipboard service](../architecture/clipboard-service.md) C0 volatile model through an injected least-authority public client seam. It lives in
`src/shell/clipboard_applet` and consumes only the typed public interface `ClipboardClientInterface`; it never contacts Wayland data devices, X11 selections, host IPC, or internal service state.

## Slice status

This WIRED slice registers its presentation projection model, controller facade, public seam adapter, compiled QML module (`QindaQt.Shell.ClipboardApplet`), hostile/boundary tests, and this documentation in the combined build graph.

The remaining integration seams are additive and listed in [Applet runtime](applet-runtime.md) terms as:

- a compiled built-in registry and QML dispatcher entry for `qindaqt.applets.clipboard`;
- a manifest catalog entry (`data/applets/clipboard.json`) declaring only the audited `clipboard.read` and `clipboard.write` capabilities; and
- shell composition that instantiates the clipboard model client adapter, injects the controller into the applet surface, and connects lock-screen privacy signals.

## Module shape

| Piece | Responsibility |
| --- | --- |
| `ClipboardAppletModel` (`clipboard_applet_model.h/.cpp`) | Pure functional projection of one history snapshot and client state into phase state, bounded rows, format summaries, accessible names, and overflow counts. No QObject, no transport, no storage. |
| `ClipboardAppletController` (`clipboard_applet_controller.h/.cpp`) | QML-facing controller over a borrowed `ClipboardClientInterface`. Reprojects on model/state changes, owns pending-request bookkeeping, validates generation fencing, and turns typed operation results into user feedback. |
| `ClipboardModelClientAdapter` (`clipboard_model_client_adapter.h/.cpp`) | Least-authority adapter implementing `ClipboardClientInterface` over the concrete C0 `ClipboardHistoryModel`. Enforces lock gating and generational fencing. |
| `QindaQt.Shell.ClipboardApplet` QML module | Token-styled presentation via `QindaQt.Controls 1.0`: main applet surface (`ClipboardApplet.qml`) plus list row component (`ClipboardEntryRow.qml`). Injected controller only. |

The controller never starts, stops, or owns the clipboard model; shell composition owns that lifecycle. QML receives no model pointers, raw payloads, or IPC endpoints.

## Presentation contract

Phases, exposed as `phaseText`:

| Phase | Meaning |
| --- | --- |
| `loading` | Client initializing or waiting for initial history snapshot. |
| `ready` | Validated ready state with active history and privacy allowed. Rows and controls are active. |
| `locked` | Screen is locked or authenticated privacy is active. Content is purged/hidden; metadata is withheld. |
| `disabled` | Clipboard history is disabled by user setting. |
| `privacyDenied` | History access is denied by privacy policy or sandbox. |
| `unavailable` | Client/service disconnected or lineage exhausted. |

`phaseReasonText` maps each phase to fixed, user-friendly explanatory sentences. Diagnostics and raw memory values are never exposed.

Rows: bounded to at most 50 entries (pinned entries first, followed by unpinned in descending recency order). Anything beyond the retention window is truncated deterministically. Each row provides:
- Entry ID string (`generation:serial`)
- Sanitized preview text with truncation indicator
- Source application label
- Formats summary and primary media type classification (`isText`, `isImage`, `isUriList`)
- Formatted total byte size
- Pinned status, admitted timestamp, and pending mutation flag
- Accessible name and description for assistive technologies

## Intent and mutation rules

Supported user operations:
- `selectEntry(generation, serial)`: Promotes the specified entry to the current active selection.
- `deleteEntry(generation, serial)`: Removes the specified entry from history.
- `togglePin(generation, serial)`: Pins or unpins an entry to prevent LRU eviction.
- `clearHistory()`: Purges all unpinned entries from history.
- `setSearchQuery(query)` / `clearSearch()`: Filters visible entries by metadata/preview matching.

All mutations enforce strict generation fencing: if the snapshot generation does not match the intent target generation, the request is refused fail-closed with user feedback and never dispatched.
