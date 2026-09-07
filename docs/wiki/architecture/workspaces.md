# Saved workspaces

A workspace keeps a container's name, chosen color, split layout, tabs and
application choices so that related work can be assembled again after login.
It describes places for windows, rather than remembering window handles from a
session that has ended. Two terminals may have different jobs; restoring a
workspace must not guess which is which.

## Current implementation boundary

`QindaQt::Workspaces` provides the saved document, atomic filesystem storage,
window assignment policy and construction of a fresh container value. It does
not yet add Save or Reopen controls to the desktop, launch applications, or
restore a running compositor session. Those remain acceptance requirements for
the reusable-workspace checkpoint in `docs/TASK_LIST.md`.

The public boundary is `qindaqt/workspaces/workspace.h` and
`qindaqt/workspaces/workspace_store.h`. Values own their data and can cross
threads by copy. `WorkspaceStore` is synchronous; its caller supplies the
storage directory and serializes access on an I/O thread. Saving uses atomic
replacement. Failed validation leaves the previous document intact; loading a
damaged or unsupported document reports an error without rewriting it.

## Names, colors and application slots

Version 1 stores a workspace identifier, a one-line name, an optional sRGB
`#RRGGBB` accent, a Core container layout and application slots. An empty color
means follow the theme. Each slot has a durable identifier, a human label, a
desktop-entry identifier and optional absolute URLs. Layout leaves refer to
slot identifiers. The sets must match exactly. A saved layout contains at
least two slots, consistent with published container ownership.

Window matching receives an explicit inventory with eligibility supplied by
the compositor. It never chooses an ineligible window. Automatic matching is
allowed only when an application has exactly one remaining slot and exactly
one eligible window. Duplicate terminals or editors require a manual choice.
An explicit choice may select a replacement application. A window cannot
occupy two slots.

Instantiation requires complete assignments and produces a new Core container
value with fresh live window identities while retaining page order, active
page, split orientation and ratios. It does not mutate the session. The
compositor must revalidate live eligibility and publish adoption atomically.

## Verification and remaining work

`workspaces.persistence-assignment` verifies persistence across store lifetimes,
last-save preservation, damaged-file reporting, schema/layout rejection,
identity rebinding, ambiguous application matches, explicit replacement and
ineligible/duplicate-window rejection. These tests do not prove logout restore.

The next integration adds capture of the active container, application launch
and missing-app reporting, a manual assignment interface, and atomic adoption.
Container name/color projection and roll-up/iconify controls are implemented
separately at the compositor boundary. Their geometry and visibility must not
be confused with the durable split layout.

See [ADR-0097](../adr/0097-separate-workspace-slots-from-live-windows.md),
[Window containers](window-containers.md) and
[Module boundaries](module-boundaries.md).
