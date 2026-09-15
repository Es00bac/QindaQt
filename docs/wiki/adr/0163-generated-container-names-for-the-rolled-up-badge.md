# ADR-0163: generated container names so the rolled-up badge is never anonymous

- **Status:** Accepted
- **Date:** 2026-09-15
- **Owners:** Platform (compositor container appearance)
- **Supersedes:** ADR-0139's badge label clause (the label format only; all
  other ADR-0139 decisions stand)
- **Superseded by:** None

## Context

ADR-0139 gave the rolled-up badge a reserved label rect (48–140 logical
pixels) and a `<container name> · <foremost tab title>` format, but the
container name exists only after an explicit rename: an unrenamed container
has no name at all, so its badge shows just the active tab's title — and in
the field the badge reads as anonymous/blank precisely for the containers
users roll up (a game in one tab, renamed never). There is no derived
container name anywhere in the session, and the rename/color store is the
established process-local boundary for container identity.

Alternatives considered: deriving the name from the active window's caption
(cycles as tabs switch, duplicates across containers, vanishes when the
caption is empty); using the application identity (wrong once the container
holds unrelated tabs); leaving the tab title as the only fallback (this is
today's behavior and the reported problem).

## Decision

Every container always has a user-visible name:

- `HybridContainerAppearanceStore::displayName()` returns the rename
  override when one exists, otherwise a stable generated `Container N`.
- Generated numbers are assigned on first observation and memoized for the
  container's lifetime; the counter is process-monotonic, so two live
  containers never share a number, and a forgotten container's number is
  never reused.
- Clearing the rename (blank setName) falls back to the memoized generated
  name; only `forgetContainer` retires it.
- The compositor passes `displayName()` into the chrome plan's
  `containerTitle` only while the container is shaded. The unshaded shared
  row keeps the override-only contract (an empty override paints no row
  title text, tabs remain the page identity), the rename prompt keeps
  prefilled with the raw override (never the generated name), and task-list
  facts keep the caption-fallback identity on purpose.

## Consequences

- The rolled-up badge always paints a real label: `Container N · <tab>`
  for unrenamed containers, `<name> · <tab>` after a rename, and the tab
  title alone only in the degenerate no-title plan input (regression-guarded
  by a pixel test).
- Naming stays process-local like rename/color; nothing new persists across
  a compositor restart and numbering restarts each session.
- A future session-restore owner should persist the generated name with the
  other appearance state; until then a restored session may renumber.

## Revisit when

- Session restore persists process-local container appearance (persist the
  generated name or derive numbering from restored topology order).
- A naming surface beyond the badge needs container identity (fold it into
  this contract rather than adding a second generator).
