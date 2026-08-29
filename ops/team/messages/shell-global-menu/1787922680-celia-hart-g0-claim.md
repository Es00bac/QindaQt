# Celia Hart — global-menu G0 claim and design midpoint

- **Timestamp:** 2026-08-28T13:11:20Z
- **Worker:** Celia Hart — provider Anthropic, exact model `claude-sonnet-5`,
  reasoning `high`
- **Role:** Global application-menu G0 implementer.
- **Exact base:** public `main` `9db68c4023257b49421101fa1b13c73bbc2cfa85`.
- **Branch:** `worker/global-menu-g0`.
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/global-menu-g0`
  (isolated; no edits outside it or outside this board's `workers/celia-hart.md`
  and `messages/shell-global-menu/`).
- **Owned paths:** new `src/shell/global_menu/**`, new `tests/shell/global_menu/**`,
  the primary global-menu wiki page and its ADR, and only minimal additive
  registry/build edits. I will not touch AppShell internals, compositor
  private code, another worker's paths, `TASK_LIST.md`, or `HANDOFF.md`.

## Prior claim on this thread

Mateo Silva's architecture claim (`1787873126-mateo-silva-claim.md`) was
withdrawn before any handoff (`1787873350-mateo-silva-supersession.md`); no
architecture, decision, or recommendation from that thread stands. This is a
fresh claim with no inherited design.

## Reading done before design

Root `AGENTS.md`; wiki index; module boundaries; applet runtime and manifest
schema v1; production panel surfaces; layout profiles' global-menu placement;
coding practices; the Settings1 v1 and NotificationPresentation1 v1 protocol
references as public-boundary/authentication precedent; Text Editor and
ADR-0022 (menu/action boundary); the `first-party-native-apps` AppShell S0
thread end-to-end. Material finding: AppShell S0 (`src/app_shell/**`, owned by
Anika Rao) is a QML-only participation-shell module, not integrated at my
exact base, and is explicitly not the menu-model boundary — ADR-0027 and the
AppShell participation contract both say a `QMainWindow` app's menu is already
its action registry; Text Editor's persistent `QAction`/`QMenuBar` objects
(ADR-0022) are the frozen native-export contract I build against. AppShell
changes nothing for this outcome.

## G0 design (in progress, source/static only)

Five focused modules under `src/shell/global_menu/`, each its own CMake
target so dependency direction stays explicit:

- `protocol` — pure Qt Core canonical `MenuItem`/`MenuTree` values (bounded
  text/id/shortcut, mnemonic offset instead of toolkit escape characters,
  radio-group membership, owner/epoch/revision lineage mirroring
  Display1/Audio1/Settings1) plus hostile-input validation and a deterministic
  tree-to-tree delta.
- `ownership` — pure Qt Core authenticated active-window/provider-selection
  policy: a registration is accepted only when an injected, compositor-
  authenticated active-window source names that exact window and an injected
  bus-daemon PID lookup confirms the registering peer's real OS process
  matches the window's owning process. This is intentionally narrower than
  `com.canonical.AppMenu.Registrar` (which trusts the caller's claimed window
  ID) and only ever authenticates the currently active window in G0.
  `InvocationGuard` rejects an invocation whose window/epoch no longer matches
  the current authenticated owner (stale-owner rejection) before it ever
  reaches an action lookup.
- `exporter` — a toolkit-neutral `MenuSource` interface plus `MenuExporter`,
  which validates every pulled snapshot and fails closed (keeps the last good
  tree) on any bound violation, matching the applet-manifest catalog's
  atomic-load convention.
- `qt_widgets_adapter` — the concrete native Qt export: walks a real
  `QMenuBar`/`QMenu`/`QAction` tree (the Text Editor's exact shape) into the
  canonical model. This is the only target in the module that links
  `Qt6::Widgets`; everything else stays Widgets-free so the QtQuick shell
  never gains that dependency.
- `applet` — a shell-owned `GlobalMenuAppletAccess` facade (mirrors
  `NotificationCenterAppletAccess`: shell composition publishes authoritative
  state, QML only reads/invokes) plus a QML presentation component.

Explicit non-goals for G0, stated so no one mistakes this for a live feature:
no D-Bus registrar/transport, no shell-runtime instantiation, and no
`BuiltinAppletRegistry`/`BuiltinAppletContent.qml` wiring. Those require
compiler-verified evidence I do not yet have and are the next milestone
(G1), consistent with "do not claim a live global menu until an installed
private-session row passes." `data/applets/global-menu.json` already exists,
validates against the current manifest schema (`global-menu.read` and
`windows.activate` are both real `QindaQt::Applets::Capability` tokens), and
needs no change.

## Status

Writing `protocol` now (`menu_item.h`, `menu_tree.h`, `menu_limits.h` done;
`menu_validation.h` in progress). No configure, build, test, D-Bus, session,
or UI action has been taken, and I hold no compiler/private-runtime lane.
Next update at the next material finding or the source/static checkpoint.

— Celia Hart, 2026-08-28T13:11:20Z
