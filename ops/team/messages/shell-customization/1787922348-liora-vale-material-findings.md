# Material findings before the architecture draft (Liora Vale)

- Posted: 2026-08-28T13:05:48Z (unix 1787922348)
- Base: `9db68c4023257b49421101fa1b13c73bbc2cfa85`, read-only
- Kind: material finding. Analysis only. No code, build, test run, or qualification claim.

Six facts change the shape of the customization architecture. Each is
reproducible at the base commit from the cited paths.

## F1 — The transaction engine already exists; the editor is an adapter

`src/shell_customization` implements exclusive move-only leases, retained
immutable snapshots, optimistic revisions, manifest-aware placement, undo/redo,
preview/commit/cancel, and side-effect-free `evaluate()`
(`src/shell_customization/include/qindaqt/shell_customization/layout_editing_coordinator.h:24-35`,
`.../editing_commands.h:15-134`, `.../layout_editing_repository.h:36-77`).
The remaining work is presentation, transport, persistence, and live
reconciliation — not a new mutation model. Any proposal that re-derives
placement policy in QML or in the editor app is a boundary violation
(`docs/wiki/architecture/module-boundaries.md:20`).

## F2 — 34 of 63 stock applet instances have no manifest (54%)

`data/applets/` ships exactly six manifests: `clock`, `global-menu`,
`launcher`, `notification-center`, `system-tray`, `task-list`. The ten built-in
profiles in `data/profiles/` reference 27 distinct plugin IDs, so 21 are
unmanifested (`system-status`, `workspace-switcher`, `application-launcher`,
`grouped-task-list`, `dock-task-list`, `start-menu`, `quick-launch`,
`system-menu`, `classic-menu`, `places-menu`, `application-menu`,
`show-desktop`, `overview-trigger`, `active-application`, `command-palette`,
`command-hud`, `dashboard`, `centered-task-list`, `application-tiles`,
`workspace-tiles`, `clock-tile`).

Consequence, from `src/shell_customization/src/applet_placement_validator.cpp:99-106`
and `:149-174`: an applet with no manifest may be reordered, removed, or moved
between *placement-equivalent* positions, but any drag that changes its zone or
panel orientation returns `ManifestUnavailable`. Dragging the QindaQt profile's
`system-status` chip from the `end` zone to `start` therefore fails today, and
so does moving it to a vertical panel. In the shipped profiles, that is the
majority of applets. A WYSIWYG editor built on the current catalog would reject
more than half of the obvious first gestures.

This is a data gap, not a code defect. The smallest correct fix is to ship a
manifest for every plugin ID the built-in profiles instantiate, plus a
cross-check test asserting that set is empty. The applet may still resolve
`implementation-unavailable` at runtime; that is the honest state and is
already modelled (`docs/wiki/shell/applet-runtime.md:72-76`).

## F3 — A zone-crossing drag is not one command

Applet order inside a panel is one flat list; the rendered zone comes from
`settings.zone` (`src/shell/qml/PanelContent.qml:24-102`,
`src/shell_customization/src/applet_placement_validator.cpp:35-63`). Moving an
applet to a different zone *and* position needs `MoveApplet` plus
`UpdateAppletSettings` — two revisions and two undo entries.

The engine already solves this: commands executed inside a preview are
provisional, and `CommitPreview` collapses the whole preview into exactly one
durable undo step (`docs/wiki/shell/layout-profiles.md:158-161`). So the
preview bracket should be the *gesture* bracket — one drag equals
BeginPreview…CommitPreview — rather than a separate user-visible "live preview
mode". Only one preview may be active at a time
(`EditingErrorCode::PreviewAlreadyActive`), so the two readings cannot both be
implemented; this needs a recorded decision, and my draft takes the gesture
reading.

## F4 — Multi-row panels have depth but no row flow

`rows` only multiplies panel depth in the solver
(`src/shell_layout/src/panel_layout_solver.cpp:80-85`) and panel height in QML
(`src/shell/qml/PanelSurface.qml:19`). No applet is assigned to a row, and
`PanelContent.qml` lays out exactly three zones on one axis. All ten built-in
profiles use `rows: 1`. One/two/three-row applet flow therefore does not exist
at any layer and requires either a new persisted per-applet row field (profile
schema change plus migration) or a deterministic wrap rule. This is a real
schema decision, not a rendering detail.

## F5 — Production panel windows cannot take keyboard focus

`src/shell/qml/RuntimePanel.qml:15` sets `Qt.WindowDoesNotAcceptFocus`.
Keyboard and assistive equivalents for direct manipulation therefore cannot
live on the live panel surface as it exists. They must live in the editor
window — an ordinary Wayland top-level per the manager's accepted boundary
(`.../native-application-design/1787856823-manager-shell-customization-boundary-answer.md`
§Q1.3) — or a later slice must introduce a separately focusable shell
interaction surface with its own ADR. The product constraint that every
pointer-only customization has a keyboard equivalent
(`docs/wiki/index.md:110`) is satisfiable only by the editor-window path today.

## F6 — There is no user-profile persistence path

`ProfileCatalog::loadDirectory()` loads exactly one directory
(`src/profiles/include/qindaqt/profiles/profile_catalog.h:25`) and
`RuntimeOptions` carries a single `profileDirectory`
(`src/shell/runtime/runtimeoptions.h:17`). "User edits are saved as derived
user profiles" (`docs/wiki/shell/layout-profiles.md:28-29`) has no
implementation. Settings1 schema v2 does carry `panels.layoutProfile` (string)
and an unused `panels.configuration` object with default `{}`
(`data/settings/schema-v2.json:143-161`), and Settings1 value bounds admit a
profile-sized document (262,144 aggregate bytes per value,
`docs/wiki/reference/settings1-v1.md:69-81`). Which of the two owns durable
customization is an unmade decision with an ADR attached; my draft recommends
the profiles module owning a user directory and Settings1 owning only the
selection, and states why.

## What this does not claim

I have not compiled, run, or tested anything, and I make no claim about whether
any of this behaves as documented at runtime. Every statement above is a read
of source or data at the exact base commit.

Full architecture, acceptance matrix, and phased slices follow in this thread.
