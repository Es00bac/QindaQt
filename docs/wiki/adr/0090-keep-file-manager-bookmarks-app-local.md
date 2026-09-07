# ADR-0090: Keep File Manager bookmarks in an app-local bounded state file

- **Status:** Accepted
- **Date:** 2026-09-06
- **Owners:** First-party applications / File Manager
- **Supersedes:** None
- **Superseded by:** None

## Context

The S2 browsing slice adds a places/bookmarks sidebar to File Manager.
Bookmarks are user content: an ordered inventory of names and absolute folder
paths that grows with use and must survive restarts. Settings1 is a closed,
schema-gated key registry for preferences and policy, not a content store;
[ADR-0065](0065-persist-text-editor-path-inventory.md) already kept the Text
Editor's path inventory out of Settings1 for exactly that reason. Writing
bookmarks through ad-hoc `QSettings` or an unvalidated JSON file would skip
the project's symlink-refusal and atomic-replacement expectations for state
files.

The owning behavior and limits are in
[QindaQt File Manager](../apps/file-manager.md).

## Decision

File Manager bookmarks persist in an application-owned file
`bookmarks-v1.json` beneath `$XDG_STATE_HOME/qindaqt-file-manager/`, owned by
`BookmarksStore` in the file-manager `model` module. The schema is exact and
validated wholesale: a version integer and a bounded array of `{name, path}`
objects (at most 128 bookmarks, 4096-char absolute clean unique paths,
256-char non-empty names, 64 KiB total). Every write is an atomic
same-directory replacement reached through `openat`/`O_NOFOLLOW`
component-wise traversal that refuses symlinked ancestors and a non-regular
final entry, mirroring the Text Editor store's contract. A missing file is a
clean first run, never an error. `PlacesController` owns add/remove/dedup/
cap policy and publishes typed store errors to presentation; it never
navigates and QML never touches the file.

View preferences (sort column/direction, hidden-file visibility, list/grid
mode) are session-local in S2 and are deliberately not persisted anywhere.

## Consequences

- Bookmark inventory survives restarts without widening the Settings1 schema
  or granting the file manager a new service authority.
- Corrupt, oversized, symlink-redirected, or hostile state degrades to a
  typed, dismissible presentation error; browsing is never blocked by it.
- Tests must cover round trips, wholesale schema/bounds rejection, symlinked
  ancestors, and the clean first-run absent case against injected roots.
- Persisting view preferences later, or syncing bookmarks through Settings1,
  requires revisiting this decision with an explicit key-schema proposal.

## Revisit when

Bookmarks need cross-device sync, per-volume or network locations make path
identity unstable, or Settings1 gains a sanctioned content-inventory key
class.
