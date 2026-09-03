# Exact-candidate review — Clipboard applet C2 production shell hosting

- Reviewer persona: **Melanie Wood**, independent shell reviewer (slug `melanie-wood`)
- Provider/model: Moonshot Kimi `kimi-code/k3`
- Candidate SHA: `a069842659e3e96dd93d0f27a66049d9d3ff03c8` (verified: `git rev-parse HEAD` before and after all work)
- Tree SHA: `8035b067fdc585161cb042596e830c529facdb77` (matches implementer handoff)
- Parent SHA: `07861e19f52a3754f4f5e19587c1a63142abbcca`
- Base SHA: `07861e19f52a3754f4f5e19587c1a63142abbcca` (parent == base; single-commit candidate)
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/clipboard-composition-k3-review` (detached at candidate; `git status --porcelain` empty before and after; no product path touched)
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-clipcomp-k3` (`<ROOT>`)

## Findings ledger

### P0 — none

No destructive, host-affecting, network, or hardware reach. The boundary gate
(`check_clipboard_applet_boundary.cmake`) was extended over the new composition
pair with a working poison probe; every test row ran under
`env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`
with display variables unset by the rows themselves. No `tests/session`
nested-compositor row was run.

### P1 — none

The composition behaves as contracted on every axis I exercised, including the
ones the candidate itself does not test (see the P2 below; my scratch
reproduction proves the behavior is correct, so this is not a P1):

- Grants are evaluated through the audited host boundary
  (`clipboardGrants`, `src/shell/runtime/clipboardappletcomposition.cpp:31`):
  manifest lookup → `HostSelector::select` → `InProcessAuditedBuiltin` +
  compiled-registry containment → `CapabilityPolicy::evaluate`; every failure
  mode falls back to `{false, false}` (fail closed).
- Owner loss clears truth: transport `ownerChanged` voids the cached wire
  snapshot and publishes `Unavailable`, clearing owner and presented state;
  the composite owner `serviceOwner|settingsOwner` requires both owners
  non-empty; Settings owner loss degrades the client and clears the same way
  (`clipboardappletcomposition.cpp:355-385`). Covered by the candidate's
  private-bus row (unregistered service → `unavailable`, 0 entries).
- Privacy-denied and ceiling-purge phases: the private-bus row drives a valid
  same-lineage purge at `UINT32_MAX` and observes `locked` →
  `lineage-exhausted-restart-required` exactly as the wiki table defines. The
  bridge's validated-transport-reply side channel for the ceiling purge is
  documented in a comment at `clipboardappletcomposition.cpp:280-287` and
  matches the C0 contract.
- Clipboard1-v1 pin requests fail closed in-process with a precise message
  without touching service internals (`requestSetPinned`,
  `clipboardappletcomposition.cpp:153-172`).
- Client completions are queued (`clipboard_client.cpp:249-254`), so the
  bridge's `dispatch()` → `m_pending.insert` ordering cannot drop a
  synchronous Busy/Rejected completion; controller request-id attribution is
  preserved.
- Destruction order is sound: `resetRuntime()` resets `m_clipboardApplet`
  before `m_settingsClient`/`m_settingsTransport`
  (`shellruntimeapplication.cpp:476-485`), and member declaration order gives
  the same default teardown; the settings client is created in
  `initializeLauncherRuntime()` strictly before
  `initializeServiceAppletCompositions()` dereferences it
  (`shellruntimeapplication.cpp:283-287`).
- Presentation: `Popup.Window` with `focus: true`, Escape/outside close,
  focus restore to the summary, always-present Close in the Tab chain,
  accessible role/name/checked state; Escape in the search field with empty
  text propagates to the popup (`ClipboardApplet.qml` edit). Verified by the
  two new offscreen rows under `QT_FATAL_WARNINGS=1` with host display/bus
  unset (16/16 clipboard selector includes them).
- Packaging: `ClipboardAppletRuntime` component + shared
  `qindaqt_install_clipboard_applet_runtime()` closure helper applied to all
  seven shell-carrying components, DesktopVirtual staging block,
  test-import stub, source-poisoned installed row — all green (see commands).
- Dispatcher inventory: applet-runtime page and launcher contract guard now
  say eight; `tst_applet_instance_resolver.cpp` pins one resolved Clipboard
  utility slot adjacent to notification center in every stock profile; all
  ten profile edits are single additive lines (verified in the diff), all
  JSON parses.

### P2 — 1 finding

**P2-1: No negative control for the Settings1 consent-denial contract.**

The candidate's headline privacy contract — module-boundaries
("admits history only with exact user-override consent",
`docs/wiki/architecture/module-boundaries.md:258-264`) and the clipboard page
("Missing, malformed, inherited-default, or ownerless consent is denied. The
shell withholds existing Clipboard1 content immediately and waits for the
service's generation-fenced empty snapshot",
`docs/wiki/shell/clipboard-applet.md:70-75`) — is implemented at
`clipboardappletcomposition.cpp:61-76` (`explicitHistoryConsent`) and
`310-385` (`publishSnapshot` denial paths `clipboard-consent-unavailable` /
`clipboard-consent-denied`). The only in-tree test that links the composition
(`tests/shell/clipboard_applet/tst_clipboard_applet_composition_private_bus.cpp`)
exercises **consent granted only**; `grep -rn "consent" tests/` finds no
denial case. The denial half of the consent gate has zero committed coverage.

Reproduction of the gap (and proof the code is currently correct — hence P2,
not P1): scratch test
`<ROOT>/scratch/tst_clipboard_consent_denial.cpp`, compiled against the Debug
tree with the same flags/objects as the private-bus test, driving the real
composition over a private bus with a parameterized fake Settings1 transport:

- consent key absent from every layer → client degrades (exact-scope check),
  applet `unavailable` / `clipboard-consent-unavailable`, 0 entries — PASS
- consent `true` from layer `system-defaults` → `unavailable` /
  `clipboard-consent-denied`, 0 entries — PASS
- consent value of wrong type (`"true"` string) with `user-overrides` layer →
  `unavailable` / `clipboard-consent-denied`, 0 entries — PASS
- consent withdrawn mid-session while Clipboard1 still reports enabled
  history with one entry → content withheld immediately: `entryCount` 1 → 0,
  phase `ready` → `unavailable`/`clipboard-consent-denied` — PASS

Command: `env -u DBUS_SESSION_BUS_ADDRESS
DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent XDG_RUNTIME_DIR=<ROOT>/scratch/runtime
QT_QPA_PLATFORM=offscreen <ROOT>/scratch/tst_clipboard_consent_denial` →
`Totals: 6 passed, 0 failed`. Observed: all denial variants fail closed as the
wiki claims; expected in-tree: at least one committed denial row (the existing
`FakeSettingsTransport` is one constructor argument away from
parameterization). Bounded repair: extend the private-bus test with a denial
case and a withdrawal case.

### P3 — none

Notes (not findings against the candidate):

- The lane's prescribed configure recipe (system-KWin cache) cannot configure
  this candidate on the current host: `src/compositor/CMakeLists.txt:86`
  pins KWin **6.6.5 EXACT** while the system provides 6.6.6. I configured both
  profiles with the common-contract recipe
  (`qindaqt-665-initial-cache.cmake`, KWin 6.6.5 private prefix) instead —
  the same recipe the implementer's acceptance evidence used. This is a lane
  instruction staleness issue, not a candidate defect.
- `src/shell/runtime/shellruntimeapplication.cpp` is now 499 non-blank lines,
  one under the 500-line decomposition-review trigger; `check-source-shape`
  reports it as an existing review-threshold warning and the handoff disclosed
  it. The next additive edit to this file crosses the trigger.
- `desktop.virtual.stage-closure` does not exist in this tree; the prescribed
  selector matched `sandbox-unit` and `package-contract` (2 rows) and both
  passed. Nested rows were not run, per the common contract.

## Commands executed and results

All ctest runs used
`env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`
from the review worktree; build root `<ROOT>` =
`/home/cabewse/work_SPaC3/builds/qindaqt/review-clipcomp-k3`.

1. Lane-prescribed configure
   `cmake -S . -B <ROOT>/dev -G Ninja -C .../qindaqt-system-kwin-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug`
   → **configure failed** (KWin 6.6.5 EXACT vs system 6.6.6). Not counted
   against the candidate; superseded by (2).
2. `cmake -S . -B <ROOT>/debug -G Ninja -C .../qindaqt-665-initial-cache.cmake
   -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON
   -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON
   -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON`
   → exit 0. Same for `<ROOT>/release` with `-DCMAKE_BUILD_TYPE=Release` →
   exit 0.
3. `cmake --build <ROOT>/debug --parallel 3 --target qindaqt-shell
   qindaqt-shell-preview qindaqt_clipboard_applet_qml_test_prerequisites
   qindaqt_clipboard_applet_{model,controller,fencing,admission,snapshot_invariant,seam,composition_private_bus,qml}_tests
   qindaqt_applet_{manifest,catalog,instance_resolver}_tests
   qindaqt_applet_host_{policy,handshake,lifecycle}_tests
   qindaqt_shell_runtime_options_tests qindaqt_notification_center_entry_tests
   qindaqt-desktop-session-probe qindaqt_desktop_entry_parser_tests
   qindaqt_application_catalog_tests qindaqt_launcher_{category_model,search_ranker,pinned_recent,presentation,scanner,composition,execution,executor,persistence,controller,qml}_tests
   qindaqt_launcher_installed_probe` → exit 0, 1649 actions.
4. Same target list in Release → exit 0, 1649 actions.
5. Debug: `ctest --test-dir <ROOT>/debug -R '^qindaqt\.clipboard-applet-'
   --output-on-failure --no-tests=error` → exit 0, **16/16** passed.
6. Debug: `ctest -R '^qindaqt\.(applet|shell-runtime-|launcher-)|^qindaqt\.notification-center-applet-offscreen$'`
   → exit 0, **27/27** passed (includes the launcher panel-dispatcher row
   under `QT_FATAL_WARNINGS=1`, the notification-center offscreen row, the
   shell-runtime component-closure and catalog rows, all applet
   manifest/catalog/resolver/host rows).
7. Debug: `ctest -R 'desktop\.virtual\.(sandbox-unit|package-contract|stage-closure)'`
   → exit 0, **2/2** passed (`sandbox-unit`, `package-contract`).
8. Release: the same three selectors → exit 0, **16/16**, **27/27**, **2/2**.
9. Debug re-run of `^qindaqt\.clipboard-applet-composition-private-bus$`
   after my scratch link transiently overwrote then ninja-relinked that test
   binary inside my own build root → exit 0, 1/1. (Scratch work never touched
   the worktree; the incident and repair are recorded for transparency.)
10. Scratch consent-denial reproduction (P2-1) → 6/6 passed, proving the
    denial paths behave correctly but are untested in-tree.
11. `./tools/validate-docs` → exit 0 (135 Markdown documents + mkdocs.yml).
12. `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build
    --strict --site-dir <ROOT>/site` → exit 0.
13. `./tools/check-source-shape` → exit 0 (only pre-existing review-threshold
    warnings; no allowlist additions).
14. `git diff --check 07861e1..a069842` → exit 0.
15. `python3 -m json.tool` over all ten changed profile JSON files → exit 0
    each.

## Verdict

The implementation is correct on every axis I could exercise, including the
consent-denial paths I built scratch reproductions for. One P2 blocks
acceptance under the review contract: the exact-`user-overrides` consent gate —
the headline privacy control this composition adds — has no committed negative
test; only the consent-granted path is covered. The repair is small and local
(parameterize the existing private-bus fake settings transport; my scratch
test demonstrates the four cases).

VERDICT REJECT P0/P1/P2/P3=0/0/1/0
