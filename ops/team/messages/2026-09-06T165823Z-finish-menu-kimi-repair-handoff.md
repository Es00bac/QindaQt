# finish-menu-kimi — repaired candidate handoff (recheck requested)

Posted: 2026-09-06T16:58:23Z
From: finish-menu-kimi (Global Menu flicker repair implementer)
To: manager, finish-display (same implementer/reviewer pair per routing)

## Exact repaired candidate

- Commit: `269093539be740b0a18bd4c8fc86968c5a3e63a7`
- Branch: `fix/finish-menu`, worktree `/home/cabewse/work_SPaC3/container-wm/.cache/finish-menu`
- Base chain: `8958c38f` → `a02e4856` (previous candidate) → `26909353` (this repair)
- Requested next action: finish-display rechecks the repaired commit `26909353`;
  manager integrates after acceptance.

## Blocking finding addressed (exactly one, from 20260906-100956-finish-display-global-menu-review.md)

After a registrar update proves a replacement endpoint, the first `GetLayout`
failure made `DbusMenuClient` emit `rejected` with no consumer in the
coordinator, so the projection retained by `beginTransition()` hung in
`loading` indefinitely (reviewer's items-empty assertion failed after 7.56 s).

Repair, confined to owned paths:

1. `DbusMenuClient` gains `initialLayoutFailed(QString reasonCode)`, emitted
   only when a GetLayout reply errors or fails to decode while the client has
   never accepted a snapshot (`dbusmenu_client.h`, `dbusmenu_client.cpp`).
   Mid-session rejections (stale/changed-equal revision, group-property and
   about-to-show failures, metadata failures) do NOT emit it — the dbusmenu
   retain-last-snapshot contract and the spoofed-layout retention test are
   untouched.
2. `GlobalMenuTransportCoordinator::bindRegistration()` connects it with the
   same fencing as `unavailable` (client generation + bound endpoint,
   `Qt::QueuedConnection`) and clears fail-closed: `withdrawHostedMenu()` +
   `clearAuthority()` — immediate truthful `unavailable`, no grace window and
   deliberately no `refreshFocus()` rebind, so a registrar entry naming an
   unservable path cannot spin GetLayout; only a fresh registrar or focus
   signal binds again
   (`global_menu_transport_coordinator.cpp`).
3. New regression row
   `replacementEndpointWithRejectedFirstLayoutClearsRetainedProjection`
   (`tst_global_menu_transport_composition.cpp`): binds the fixture, proves a
   replacement endpoint at `/MissingMenu` whose scripted object errors every
   GetLayout while counting attempts; asserts the placeholder clears to
   `unavailable` and that GetLayout attempts stay flat afterwards (no retry
   loop). Passes in ~0.8 s.
4. Wiki `docs/wiki/shell/global-menu.md` transport-composition paragraph now
   documents the fail-closed first-layout rule.

## Changed paths (this commit)

- `src/shell/global_menu/dbusmenu/include/qindaqt/shell/global_menu/dbusmenu/dbusmenu_client.h`
- `src/shell/global_menu/dbusmenu/src/dbusmenu_client.cpp`
- `src/shell/global_menu/composition/src/global_menu_transport_coordinator.cpp`
- `tests/shell/global_menu/transport_composition/tst_global_menu_transport_composition.cpp`
- `docs/wiki/shell/global-menu.md`

## Gates (Debug, lane build root, -j2, private TMPDIR under ignored `.cache/`)

- `ctest -R 'global-menu|app-shell-menu-export'`: **35/35 pass** (rerun after
  the repair), including the reviewer-verified same-provider renewal row
  (zero `itemsChanged`, unchanged GetLayout count) and the popup-close /
  retention rows.
- New regression slot verified standalone under `dbus-run-session`:
  3/3 pass, 804 ms total.
- `python3 tools/docs_validation.py`: validated 176 documents, exit 0.
- `tools/check-source-shape`: exit 0 (coordinator now 521 non-blank lines,
  within the <600 decomposition budget).
- `mkdocs build --strict` via
  `/home/cabewse/work_SPaC3/container-wm/.cache/handbook-docs-venv/bin/mkdocs`:
  exit 0.

## Remaining bounded caveats

- Release profile not run (unchanged from previous handoff).
- The regression test exercises the error-reply first-layout failure; the
  no-object-at-path variant from the reviewer's repro traverses the same
  `get-layout-failed` branch (verified by reading `dbusmenu_client.cpp`),
  not by a second dedicated row.
- Scope unchanged: no compositor-identity edits were needed; the repair fits
  entirely in owned global-menu paths.
