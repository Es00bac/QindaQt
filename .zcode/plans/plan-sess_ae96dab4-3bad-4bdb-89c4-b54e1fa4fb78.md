Plan for three features: container aspect-ratio lock, rolled-up strip naming, and saved-layout reopen with launcher placeholders.

Repo policy (AGENTS.md) applies throughout: wiki pages + ADRs updated in the same change as behavior, `AGENT-*` comment markers for traps, focused tests per module plus the broadest suite, milestone commits per completed outcome. Next free ADR numbers: 0162–0165.

---

## Feature 1 — Aspect-ratio lock on a container

**Semantics.** A container-wide lock, set from the group context menu (submenu "Aspect ratio": *Unlocked / Lock current / 16:9 / 4:3 / 21:9 / 1:1*, checkmark on active). The pinned ratio applies to the container's **content area** (outer frame minus chrome: `2*outerBorder + titleBarHeight`, matching the `expectedContentRect` contract in `hybridchromeplanbuilder.cpp:121-131`), because that is the rectangle the game actually occupies. Pointer and keyboard outer resizes keep the ratio; maximize intentionally ignores it (fills the work area; restore frame keeps it); resize is already rejected while shaded; divider drags are unaffected (intra-page). State is process-local like rename/color/maximize-restore frames (nothing survives restart today; noted as future persistence work).

**Changes.**
1. `src/compositor/kwin/hybridcontainerplacement.h/.cpp` — `QHash<QString, double> m_aspectPins` + `setAspectRatioPin(id, std::optional<double>, error)` / `aspectRatioPin(id)`; cleared in `forgetContainer`. Extend `resizedFrame()` (`hybridcontainerplacement.cpp:346-381`): after the existing min-clamp, if pinned, the dragged axis leads (width leads on corner drags) and the follower content extent is derived from the ratio, anchored at the non-dragged (or baseline-fixed) edge; re-apply `MinimumOuterWidth/Height` clamps with the ratio recomputed from the clamped minimum so min sizes always win. Integer rounding: ratio holds within ~1 px (tests assert with tolerance). Use default `HybridChrome::ChromeMetrics` for the chrome offsets with the same "kept in sync" AGENT-NOTE precedent as `shadedOuterHeight()`.
2. `src/compositor/kwin/kwingroupcontextmenu.h/.cpp` — new `GroupContextMenuCommandKind` + submenu with the six entries and state provider reporting the active pin (verify submenu support; add if the menu builder lacks it).
3. `src/compositor/kwin/kwinhybridcontextmenu.cpp` — session-side dispatch to the placement controller (session-policy routing pattern used by `SetKeepAbove`/rename).
4. Tests: `tests/compositor/tst_hybridcontainerplacement.cpp` — edge drag keeps ratio, corner drag leads with width, minimums beat ratio, cancel restores baseline, unpinned behavior unchanged, keyboard path composes. Menu state/dispatch tests alongside existing context-menu tests.
5. Docs: `window-containers.md` container-behavior bullet; **ADR-0162** (content-area ratio semantics, process-local, menu UX, maximize exception).

## Feature 2 — Container name in the rolled-up strip

**Findings.** The badge already composes `"<container name> · <active tab title>"` (`chromeshadedbadge.cpp:145-147`), but the name only exists after an explicit rename and you report the label is blank today — so there is both a **root-cause bug to find** and a **missing default name**. Per your answers: unrenamed containers get a generated **"Container N"** name.

**Changes.**
1. `src/compositor/kwin/hybridcontainerappearance.h/.cpp` — generated default names: monotonic per-session counter, `displayName(containerId)` = rename override else `"Container N"`, stable for the container's lifetime, forgotten in `forgetContainer`.
2. Badge label becomes `displayName + " · " + tabTitle` (elided as today). Scoped to the container chrome surfaces (badge + shared-row rename flow); task-list/dock titles keep the current caption-fallback behavior on purpose.
3. **Root-cause the blank label** in the nested harness (`tests/session/shade_visibility`): candidates include identity `badgeInk` resolving invisible for the default (un-renamed) identity, or the label gate at `chromeshadedbadge.cpp:148`. Acceptance: pixel proof that the label paints before/after, for a default-identity container.
4. Tests: `tests/hybrid_chrome/tst_shadedbadge.cpp` (generated name always visible, ink assertion under default identity), plan-builder and renderer rows.
5. Docs: shade bullet in `window-containers.md`; **ADR-0163** superseding ADR-0139's label clause (generated naming).

## Feature 3 — Reopen saved layouts with launcher placeholders

**Architecture.** Layout leaves must be live windows, so a "placeholder" is a **real QindaQt File Manager window in a picker mode** occupying the slot. Restoring binds picker windows like any other window (no Hybrid/topology change on the restore path, and the ≥2-member rule holds since workspaces require 2–128 slots). When you launch an app from the picker, the **compositor** launches it (via the existing `WorkspacesApps::DesktopApplications`, with activation token), then swaps the picker leaf for the new app's window in one atomic topology transaction, and the picker closes on the confirmed swap.

**Phase 3a — shared application catalog + FM Applications browser.**
1. New Qt-Core-only module `src/application_catalog` porting the shell launcher's pure pieces (`DesktopEntryParser`, `ApplicationScanner`) plus a new nested **category-tree builder** (XDG main categories top-level, registered subcategories nested, `Categories=`-derived, unmapped→Other, `NoDisplay/Hidden` filtered) and the pure launch planner from `LaunchExecutor` (argv/DBusActivatable/Terminal planning with injected spawner). The shell launcher links this lib instead of its own copies (behavior unchanged, existing tests keep passing). Chosen over KService/KServiceGroup: keeps the L0-purity rule, no new KDE dependency in apps, deterministic without KSycoca; full menu-spec `.menu`/`.directory` merging explicitly out of scope (documented approximation, matching the existing launcher precedent).
2. File manager: `ApplicationsController` in `model/`, an "Applications" place in `PlacesSidebar`, `ui/ApplicationsView.qml` (folder-drill navigation, Finder style), action-catalog entries, `CMakeLists.txt`/`main.cpp` wiring. Standalone browsing launches apps through the ported planner + QProcess (widens ADR-0029's launch contract via the new ADR).
3. Tests: catalog parser/tree units with fixture `.desktop` sets, FM model + QML viewport tests, boundary poison checks, desktop-metadata check updates. Docs: `file-manager.md`, `module-boundaries.md`, `launcher.md`; **ADR-0164**.

**Phase 3b — picker mode, reopen flow, compositor swap.**
1. FM picker mode: `--choose-application` launch flag (today's folder-only arg handling exits 4; extend `main.cpp`; update `.desktop` Exec and metadata checks). Same Applications view in chooser mode: activating an entry calls the compositor and waits for the result signal; Esc cancels.
2. Reopen dialog (`src/workspaces_ui/src/workspace_dialogs.cpp`): per-slot **"Choose later"** option (your chosen default: saved apps reopen by default, any slot can become a placeholder at reopen time — no persisted slot kind, so the workspace schema stays v1). "Choose later" launches a picker via `KWinWorkspaceUiPort` (reusing the `launchFinished`/refresh/bind machinery); pickers appear in the slot combos and bind by explicit assignment, preserving the "never guess" workspaces rule.
3. Compositor D-Bus: extend `KWinControlEndpoint` (`org.qindaqt.Compositor1`) with `ChooseApplicationForWindow(window, storageId)` + a result signal; production-enabled but narrowly validated (the named window must be a live container member). **ADR-0165** for the process-boundary/security posture.
4. Replacement: pending-replacement registry in the runtime (storage id → placeholder member, ~15 s expiry), correlated on window-arrival via the existing `workspaceDesktopEntryId` rule; a new typed command `ReplaceMemberWindow{containerId, placeholderWindowId, incomingWindowId}` (`topologycommand.h` + mutation alongside `topologyadoptionmutation.cpp` + coordinator dispatch + runtime entry) rebinds the leaf, preserving node ids and ratios, atomic publish with rollback. Edge cases: app never maps (timeout → picker shows error, stays open), second window (bind first eligible, drop pending), picker closed manually (drop pending), container already unwrapped (window opens normally).
5. Tests: `tests/hybrid/tst_topology_commands.cpp` + adoption-style mutation tests, `tests/compositor/tst_hybridinteractionruntime.cpp`, endpoint tests, extend `tests/session/WorkspaceReopenTests.cmake` two-session scenario with a picker replacement; FM arg tests.
6. Docs: `workspaces.md`, `file-manager.md`, `module-boundaries.md`, `testing-harness.md`; **ADR-0165**.

---

## Out of scope (stated deliberately)

- Aspect lock on independent single windows (a one-member container is unwrapped by invariant 6; the game must share a container with at least one other member/tab — matching your "only item in a tab" framing).
- Persistence of pins/names across compositor restart (nothing persists today; flagged for the future persistence owner).
- Full XDG menu-spec merging and restructuring the shell launcher UI onto the nested tree (follow-up).

## Order and verification

Land as four milestone commits: (1) aspect lock, (2) strip naming incl. blank-label root cause, (3) catalog + FM Applications browser, (4) picker reopen + compositor swap. Each: module build + focused tests, then the broadest suites (core/hybrid/compositor/apps ctest), nested-harness scenarios for compositor/FM changes, `mkdocs build --strict` + link checker, and the ADR/wiki updates in the same change.