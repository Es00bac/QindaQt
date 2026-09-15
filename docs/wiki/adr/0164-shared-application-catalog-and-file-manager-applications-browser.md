# ADR-0164: one shared application catalog behind the File Manager's Applications browser

- **Status:** Accepted
- **Date:** 2026-09-15
- **Owners:** First-party applications (File Manager) with the Shell working
  group (launcher L0)
- **Supersedes:** None
- **Superseded by:** None

## Context

Saved workspace layouts need to reopen without launching every application
again; the agreed shape is a slot placeholder that shows a macOS-Finder-style
application picker and is replaced by the launched application (ADR-0165).
Both the picker and standalone browsing need one installed-application view:
validated desktop entries, a nested category hierarchy like the standard
Linux application menu, and launch planning. The shell launcher already owns
the hardened pure pieces — the desktop-entry parser, the L0 catalog builder,
and (until now, private to its runtime) the bounded execution-key grammar —
while the launcher's presentation (twelve fixed flat groups) deliberately
flattens hierarchy. Duplicating any parsing or planning in a second app would
create a second authority over hostile desktop-entry input.

Alternatives considered: linking the File Manager to the shell launcher
runtime (couples an apps process to shell Settings/DBus machinery it cannot
use); adopting KService/KServiceGroup (adds a KDE-framework dependency to the
apps side and to KSycoca indexing behavior, against the module boundary that
scopes the File Manager's KDE usage to private KIO); copying the parser
(forbidden — second authority).

## Decision

1. **New apps-side module `src/application_catalog`** (`QindaQt::ApplicationCatalog`,
   Qt Core only, static, installed) depends on the launcher's pure L0 and
   provides:
   - `scanApplicationDirectories()` — a synchronous, bounded scan of the
     `applications/` tree of caller-injected XDG data roots (root precedence,
     id derivation, ceilings, and hostile-input rules identical to the shell
     launcher's scanner, which stays the launcher's own watcher-based
     provider), retaining each winning document's text and path;
   - `buildCategoryTree()` — the deterministic nested tree: top-level folders
     are the launcher's twelve fixed presentation groups; an entry's
     additional *registered* XDG categories (Desktop Menu Specification
     additional-category list) become child folders under its primary group;
     each entry appears exactly once; empty folders are pruned. Full
     menu-spec `applications.menu`/`.directory` merging remains out of scope —
     this is the documented approximation shared with the launcher's flat
     grouping;
   - `planApplicationLaunch()` — pure launch planning over a retained
     document (typed support classification: process spawn, terminal
     required, D-Bus activatable, unsupported). The dependency direction is
     apps module → launcher L0 only; the launcher never links the new module.
2. **The execution-key grammar becomes launcher L0 public API.**
   `LaunchExecutionParser`, `ExecFieldCodeExpander`, `ExecPlan`, and bounds
   move from the launcher runtime's private headers to
   `qindaqt/shell_launcher/launch_execution.h`. They were already pure text
   parsing; this changes visibility, not the ADR-0042 contract that L0 never
   executes anything.
3. **The File Manager gains an Applications browser** (`go.applications`,
   Ctrl+Shift+A): the `ApplicationsController` scans the composition root's
   data roots, exposes folders/entries/breadcrumb, and launches. Launch
   policy widens ADR-0029 narrowly: ADR-0029's `QDesktopServices::openUrl`
   path for *documents* is untouched; activating an *application entry* whose
   planned argv is a plain process starts that argv directly via
   `QProcess::startDetached`. Terminal-required and D-Bus-activatable entries
   stay non-launchable here and report a typed message pointing at the
   workspace picker route (ADR-0165), where the compositor owns the full
   desktop-entry launch facility. File Manager still owns no MIME database
   and never opens local file URLs from this view.

## Consequences

- One parsing/planning authority feeds the shell launcher, the shell icon
  resolver, and the File Manager browser; hostile-input ceilings stay single.
- The Applications browser works offline, needs no bus, and shows the same
  catalog truth as the launcher while adding the nested hierarchy the
  launcher's flat presentation intentionally lacks.
- The launcher's public install grows one header; consumers that previously
  included the runtime's private `launch_execution.h` now include the public
  path (the launcher runtime and its tests are updated in the same change).
- Terminal and D-Bus-activatable applications cannot be started from
  standalone browsing until the workspace picker route (or a later widening)
  provides their launch path; the UI says so instead of failing silently.
- Scan diagnostics (unreadable roots, ceiling hits) surface as one bounded,
  dismissible summary, mirroring the launcher's degrade-not-fail rule.

## Revisit when

- A second apps consumer needs terminal/D-Bus activation launches (extend
  the shared launch planning with an injected spawner/activator seam then).
- The desktop moves to full menu-spec merging (the tree builder is the single
  place to change).
- Session restore needs persisted application identity for pickers
  (ADR-0165 covers the restore-side contract).
