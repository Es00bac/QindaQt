# ADR-0064: Persist only Text Editor's bounded path inventory

- **Status:** Accepted
- **Date:** 2026-09-03
- **Owners:** First-party applications / Text Editor
- **Supersedes:** The single-document hosting consequence of ADR-0022 only
- **Superseded by:** None

## Context

Text Editor now hosts multiple local documents and may optionally reopen them
after restart. The per-document atomic persistence and external-revision
guarantees in [ADR-0022](0022-keep-text-documents-local-and-atomic.md) must
survive that hosting change. Restoring dirty buffers would introduce content
journaling, recovery conflict policy, and a new store of user content. Putting
an open-document inventory in Settings1 would likewise turn a Boolean policy
authority into content-bearing application state.

The owning behavior and limits are in
[QindaQt Text Editor](../apps/text-editor.md).

## Decision

One editor window may own at most 32 independent document controllers. A
canonical local path is unique within that collection. Every controller retains
its own store, exact byte revision, dirty and external state, watcher, and undo
history; collection hosting does not weaken ADR-0022.

Settings1 persists only the Boolean
`services.textEditorRestoreDocuments`, default `false`, with the existing
snapshot, conflict, and uncertain-write semantics. When confirmed enabled, an
editor-owned state file beneath `$XDG_STATE_HOME` atomically stores only a
version, a bounded unique list of absolute paths, and the active path index.
The schema is exact and validated wholesale. Its filesystem boundary walks
every directory component with `openat`/`O_NOFOLLOW`, retains the opened final
directory through atomic replacement, and refuses symlinked ancestors or an
unsafe final entry. A valid inventory is only a list of open attempts: each
path must still pass normal document admission, and an unavailable target is
skipped with one bounded accessible notice.

No document content, dirty state, selection, history, encoding metadata, byte
revision, or recovery payload may enter Settings1 or the restore file. Explicit
CLI paths suppress restore for that launch.

## Consequences

- Multi-document UI preserves the same atomic-save and external-change safety
  independently for every tab.
- Restart may reopen only the current bytes of readable disk files. It cannot
  recover unsaved work and never claims to.
- Disabling the policy clears the local path inventory; unavailable Settings1
  truth behaves as disabled.
- A symlinked state-directory ancestor cannot redirect inventory I/O outside
  the selected state path; the operation fails closed instead.
- Conflicts require explicit retry and uncertain writes are never replayed.
- Tests must prove canonical uniqueness, per-tab state/undo isolation, exact
  paths-only schema and bounds, malformed/symlink rejection, skipped targets,
  and Settings1 loss/conflict/uncertainty behavior.
- Autosave, journals, revision history, remote documents, and collaborative
  editing remain explicit future decisions.

## Revisit when

QindaQt accepts crash recovery or any content-bearing editor persistence, needs
restore across remote-document identities, or replaces local optimistic saves
with a different versioned storage authority.
