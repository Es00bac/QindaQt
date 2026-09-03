# Independent exact review — Global Menu G1 production transports

- **Reviewer persona:** Elizabeth Feinler (`elizabeth-feinler`), independent shell-transport reviewer
- **Provider/model:** Moonshot Kimi `kimi-code/k3`, reasoning high
- **Candidate SHA:** `7c27ee5b1b50746e59f70360d89b0e959328dd47`
- **Candidate tree SHA:** `22faf3384afd57dd41f23f560725f605c1fc198f` (matches handoff)
- **Parent SHA:** `74da46345c7a5094d45c756ad8b23ca87591fcd3` (sole parent, matches handoff)
- **Base SHA:** `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- **Review worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/global-menu-g1-k3-review`
  (detached at candidate; `git rev-parse HEAD` re-verified equal to the candidate SHA and
  `git status --porcelain` empty both before and after all work; no product path was touched)
- **Build root:** `/home/cabewse/work_SPaC3/builds/qindaqt/review-g1-k3`
- **Implementer handoff reviewed:** `a915315:ops/team/messages/shell-global-menu/1788405440-radia-perlman-handoff.md`
  and midpoint `1788404932-radia-perlman-midpoint.md`

## Findings ledger

### P0 — none

### P1 — none

### P2 — none

### P3

**P3-1: testing-harness page claims "lower revision rejection" coverage that no candidate test row exercises.**

- Location: `docs/wiki/development/testing-harness.md` (added section "Current global-menu transport
  proof") says the dbusmenu rows cover "lower and changed-equal revision rejection".
- Observation: the changed-equal replay case *is* covered
  (`tests/shell/global_menu/dbusmenu/tst_dbusmenu_client.cpp:61-64`,
  `tests/shell/global_menu/transport_composition/tst_global_menu_transport_composition.cpp:129-133`),
  but no row drives a `GetLayout` reply whose revision is strictly below the accepted high-water mark,
  which is the `revision < m_remoteRevision` → `stale-layout-revision` branch in
  `src/shell/global_menu/dbusmenu/src/dbusmenu_client.cpp:260-263`.
- Impact: nonblocking. The branch is proven correct by independent scratch reproduction (below):
  a hostile exporter serving revision 2 after revision 5 was accepted produced exactly
  `rejected("stale-layout-revision")`, left the high water at 5, retained the accepted snapshot, and a
  stale `LayoutUpdated(2)` announcement triggered no reread. Recommend one added row or a doc wording
  fix in a follow-up; no code defect.

## Review questions

### 1. Registrar ownership and authority — verified

- The well-known name `com.canonical.AppMenu.Registrar` is requested only in
  `AppMenuRegistrar::start()` on the injected connection
  (`src/shell/global_menu/registrar/src/appmenu_registrar.cpp:41-65`); construction has no side
  effect, partial startup rolls back (`stop()` on `registerService` failure), and no global or
  ambient `QDBusConnection::sessionBus()` exists anywhere in `src/shell/global_menu` (enforced by the
  self-proving poison test, and confirmed by inspection).
- Registrations are keyed to the D-Bus message's exact caller unique name
  (`appmenu_registrar_object.cpp:36` → `registrar_registry.cpp:82-141`); non-DBus callers resolve to
  an empty name and fail closed as `Invalid`. Unique-name syntax is validated
  (`registrar_registry.cpp:14-39`).
- Unregister succeeds only for the registering owner (`NotOwner` otherwise) or via generation-fenced
  owner loss (`retireOwner`, `registrar_registry.cpp:173-195`, with an accurate AGENT-GUARD).
- Duplicate/foreign name ownership is truthful: `NameAlreadyOwned` with the squatter left
  non-running; no silent takeover.
- Hostile reproduction (scratch, private `dbus-run-session` bus, binary
  `review-g1-k3/scratch/hostile_second_owner`, exit 0, 9/9 PASS): second fake owner's
  `RegisterWindow(77, /EvilMenu)` rejected over the bus with the registry unchanged (owner and path
  intact); second-owner `UnregisterWindow(77)` rejected; window id 0 rejected; malformed path
  rejected; duplicate registrar name reports `NameAlreadyOwned`.
- Candidate's own rows cover the same at registry and bus level
  (`tests/shell/global_menu/registrar/tst_appmenu_registrar.cpp`), including owner-loss retirement
  and capacity.

### 2. dbusmenu decoding bounds — verified

- Depth (`kMaxDepth`), children-per-item, total items, per-item property count (32), label bytes,
  icon name (256 B), icon data (256 KiB), shortcut sequence/token bounds, closed `type`/
  `children-display`/`toggle-type`/`toggle-state` value sets, and well-formed-Unicode checks are all
  enforced in `src/shell/global_menu/dbusmenu/src/dbusmenu_decoder.cpp` with whole-layout atomic
  rejection; `decodeLayout` additionally routes through G0 `validateMenuTree` (which rejects
  duplicate ids, `menu_validation.cpp:65-68`).
- Unknown properties are ignored within the bounded map; known properties with wrong variant types
  reject (`invalid-item-properties`).
- Scratch reproduction (`review-g1-k3/scratch/hostile_decoder_ids`, exit 0, 7/7 PASS) covered the
  attacks the candidate's own decoder test does not: negative id → `invalid-item-id` with empty
  snapshot (no partial publication); zero id rejected; duplicate ids → `duplicate-id`; wrongly typed
  child variant → `invalid-child`; one hostile sibling rejects the whole layout; sane control
  accepted.
- Stale revisions: per-owner high water plus generation+serial fencing on every pending call
  (`dbusmenu_client.cpp:238-273`); equal-revision/changed-content is contradicted and rejected with
  the accepted snapshot retained (candidate tests), and strictly-lower replies are rejected without
  publication (scratch `review-g1-k3/scratch/stale_revision`, exit 0, 8/8 PASS — see P3-1 for the
  coverage nit). Signals are invalidation hints only; unrevisioned `GetGroupProperties` never
  overwrites the snapshot (`dbusmenu_client.cpp:334-337`).
- Note: the empty-label rejection (`invalid-label`) matches the canonical model, which itself
  rejects empty non-separator text (`menu_validation.cpp:92`) — consistent, not a defect.

### 3. Lineage — verified

- The coordinator joins the numeric registrar id only through the injected
  `RegistrarWindowIdSource`, re-runs G0 `ProviderAuthenticator` (PID/unique-name/focus proof), and
  only then adopts into `ActiveProviderSelector`
  (`global_menu_transport_coordinator.cpp:39-105, 107-146`). Each accepted remote layout is
  re-authenticated and re-adopted (fresh selector revision) before the unchanged G0 exporter stamps
  lineage through `lineageFor` (AGENT-GUARD at lines 134-136); the remote revision never becomes
  invocation authority.
- The applet snapshot therefore can never show a menu whose proof lapsed: focus loss, registration
  replacement, owner loss, or generation mismatch all route to `clearAuthority()` →
  `publishUnavailable()`; the composition test proves epoch preservation across path replacement and
  unavailable publication on focus withdrawal and owner loss.
- Activation captures the published tree synchronously, runs `InvocationGuard` on that captured
  lineage, converts the canonical id back to the dbusmenu id with a round-trip check, and submits
  exactly one `clicked` `Event` per applet signal (`global_menu_transport_coordinator.cpp:148-180`).
  `DbusMenuClient` never retries an uncertain `Event` (AGENT-GUARD `dbusmenu_client.cpp:392-397`),
  and late replies are fenced by the owner generation check in every watcher callback. The
  composition test counts exactly one `Event` observed for one activation.

### 4. Tests — verified non-vacuous and private

- All five new rows exercise candidate-only headers/libs (they cannot build, let alone pass, on the
  base tree), and assertions are behavioral with exact outcomes/reason codes (e.g. spoofed
  unregister yields an error and the registration survives; spoofed equal-revision content leaves
  the applet showing the old menu; exactly one event per activation; epoch equality across
  replacement). The boundary-poison row plants each of the four poisons and fails unless each is
  rejected (`tests/shell/global_menu/boundary/check_global_menu_transport_boundary.py:49-58`).
- Every bus-bearing row runs under `dbus-run-session` with per-test named connections
  (`tests/shell/global_menu/registrar/CMakeLists.txt:11-15`,
  `tests/shell/global_menu/dbusmenu/CMakeLists.txt:30-34`,
  `tests/shell/global_menu/transport_composition/CMakeLists.txt`); the host session bus is never
  addressed. No `tests/session` row, host service, hardware, uinput, or network was run by me
  either.

### 5. Boundaries and docs — verified

- `git diff 74da463..7c27ee5 -- src/shell/runtime src/shell/qml src/applet_runtime src/applets
  data/applets` is empty. Shared-registry edits (`mkdocs.yml`, `docs/wiki/adr/index.md`,
  `docs/wiki/architecture/module-boundaries.md`, the two global-menu `CMakeLists.txt`) are purely
  additive. New module link sets match their new module-boundary rows exactly (registrar:
  ownership+Qt Core/DBus; dbusmenu: protocol/exporter+Qt Core/DBus; composition: public
  applet/dbusmenu/exporter/ownership/registrar boundaries).
- Wiki and ADR-0056 are truthful about scope: no production-shell instantiation, no applet
  registry/manifest wiring, no installed QML module, no submenu popups, no live-desktop claim;
  caveats match the code. AGENT markers are present where the invariants live (generation fencing,
  Event no-replay, re-authentication before stamping, G1 boundary note).
- Handoff evidence claims (16/16 Debug and Release, all static gates) reproduced exactly; the
  interim 4/5 failure it discloses is plausible test-authoring history and does not affect the
  immutable candidate.

## Commands executed and results

From the worktree root `/home/cabewse/work_SPaC3/container-wm-workers/global-menu-g1-k3-review`,
build root `<ROOT>=/home/cabewse/work_SPaC3/builds/qindaqt/review-g1-k3`:

1. `git rev-parse HEAD` → `7c27ee5b1b50746e59f70360d89b0e959328dd47`; `git status --porcelain` →
   empty (before and after); `git rev-parse 7c27ee5^{tree}` →
   `22faf3384afd57dd41f23f560725f605c1fc198f`; `git rev-parse 7c27ee5^` →
   `74da46345c7a5094d45c756ad8b23ca87591fcd3`.
2. Debug configure (lane recipe, exact cache/options): exit 0.
3. Debug build, 14 global-menu library/test targets
   (`cmake --build <ROOT>/debug --parallel 3 --target qindaqt_global_menu_registrar
   qindaqt_global_menu_dbusmenu qindaqt_global_menu_transport_composition
   qindaqt_global_menu_registrar_tests qindaqt_global_menu_dbusmenu_decoder_tests
   qindaqt_global_menu_dbusmenu_client_tests qindaqt_global_menu_transport_composition_tests
   qindaqt_global_menu_protocol_tests qindaqt_global_menu_ownership_tests
   qindaqt_global_menu_lineage_tests qindaqt_global_menu_exporter_tests
   qindaqt_global_menu_qt_widgets_adapter_tests qindaqt_global_menu_applet_access_tests
   qindaqt_global_menu_composition_tests`): exit 0. (An earlier attempt with guessed target names
   failed with "unknown target"; corrected to the real names above.)
4. `ctest --test-dir <ROOT>/debug -R '^qindaqt\.global-menu-' --output-on-failure --no-tests=error`:
   exit 0, **16/16 passed** (tests #123–138).
5. Release configure (same recipe, `-DCMAKE_BUILD_TYPE=Release`): exit 0 (only the repository's
   existing mixed-prefix safe-RPATH warnings).
6. Release build, same 14 targets: exit 0 (101 actions).
7. `ctest --test-dir <ROOT>/release -R '^qindaqt\.global-menu-' --output-on-failure
   --no-tests=error`: exit 0, **16/16 passed**.
8. `./tools/validate-docs`: exit 0, 117 Markdown documents + `mkdocs.yml` navigation validated.
9. `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir
   <ROOT>/site`: exit 0.
10. `./tools/check-source-shape`: exit 0, 1,789 files checked, 0 skipped; only the two pre-existing
    review-threshold warnings (`tests/compositor/CMakeLists.txt`,
    `tests/services/display_color_model/tst_color_model.cpp`); no candidate-owned file listed.
11. `git diff --check 74da463..7c27ee5`: exit 0. Changed-JSON count: 0 (no JSON gate applicable).
12. Scratch reproductions (built against the Debug libraries, sources and binaries under
    `<ROOT>/scratch/`, run under `dbus-run-session`, all exit 0):
    - `hostile_second_owner`: 9/9 PASS (cross-owner register/unregister refused over the bus, no
      silent takeover, window 0 and malformed path refused, duplicate name → `NameAlreadyOwned`).
    - `hostile_decoder_ids`: 7/7 PASS (negative/zero/duplicate ids and wrongly typed children
      rejected atomically, no partial model; sane control accepted).
    - `stale_revision`: 8/8 PASS (strictly-lower `GetLayout` reply rejected as
      `stale-layout-revision`, high water and accepted snapshot retained, stale `LayoutUpdated`
      ignored without reread).

Unavailable coverage: none within the lane's permitted scope; `tests/session` nested-compositor
rows were not run, per the ground rules.

## Verdict

The candidate satisfies every review question with executed evidence. One nonblocking P3
doc/coverage precision item (P3-1). P0 = P1 = P2 = 0.

**ACCEPT.**
