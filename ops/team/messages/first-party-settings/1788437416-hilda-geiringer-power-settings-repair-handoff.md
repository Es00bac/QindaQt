# Power Settings rejection repair handoff — Hilda Geiringer

- Candidate commit: `0392aee499e1de3426cd953a61694ed04f1facb9`
- Candidate tree: `b2b217c7dcd2902d4b4e3e61f53770bfd5d5bc73`
- Candidate parent / repair worktree base: `8002fa562b813813bd106dca776862ce566a8ad6`
- Rejected product candidate repaired: `13a0870433f5a101af9a5d860a9690599ba5391f`
- Original lane base: `347d32f92b32c57edd4fe055c26aab8a8f27298e`
- Branch: `worker/power-settings-route`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/power-settings-route`

## Finding closure

- **P2-1** closes in `0392aee499e1de3426cd953a61694ed04f1facb9`.
  `PowerSettingsModelTest::staleSnapshotClosesPresentationAndAdmission`
  reproduces the reviewer's failed-newer-revision fetch, requires every
  retained domain control to be disabled, and requires forced profile and
  keyboard-brightness requests to be refused without transport submission.
  Its `AGENT-NOTE: P2-1 regression` names the finding. The shared base
  predicate now also requires the public client itself to be `Ready` or
  `Degraded`, so stale retained rows cannot remain actionable.
- **P2-2** closes in `0392aee499e1de3426cd953a61694ed04f1facb9`.
  `PowerSettingsSliderTest::unchangedNormalizedAndRawEquivalentDispatchNothing`
  has `exact-normalized` and `same-raw-after-conversion` data rows, returns an
  already queued gesture to current truth, and requires zero submissions. Its
  `AGENT-NOTE: P2-2 regression` names the finding. The model cancels a queued
  same-row gesture when either the normalized value is unchanged or conversion
  yields the current raw value; dispatch retains a final raw-equality guard.
- **P3-1** closes in `0392aee499e1de3426cd953a61694ed04f1facb9`.
  `PowerSettingsModelTest::successfulRetryClearsReconnectStatus` performs the
  stop/start retry, announces the owner, publishes a fresh accepted snapshot,
  and requires ready truth with empty reconnect and error text. Its
  `AGENT-NOTE: P3-1 regression` names the finding. Explicit retry lifecycle
  state clears only after a current `Ready` or `Degraded` snapshot exists.

## Rejected-candidate reproduction

Before the product repair, the worktree's product paths were byte-for-byte the
rejected `13a0870` tree (only its already committed coordination commit was on
top). After adding the three registered regression cases and before editing
production code, I ran:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/power-settings-route/debug --parallel 3 --target qindaqt_power_settings_model_tests qindaqt_power_settings_slider_tests
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/power-settings-route/debug -R '^qindaqt\.settings-power-(model|slider-admission-debounce)$' --output-on-failure --no-tests=error
```

The build exited 0. CTest exited nonzero with **0/2 rows passing**. The model
row failed P2-1 at the first enabled stale profile control and P3-1 at the
persisting reconnect text (5 QtTest cases passed, 2 failed). The slider row
dispatched once in both unchanged data rows (4 QtTest cases passed, 2 failed),
reproducing P2-2 through exact normalized and raw-equivalent paths.

## Changed product paths

- `docs/wiki/apps/power-settings.md`
- `docs/wiki/development/testing-harness.md`
- `src/apps/settings/power/include/qindaqt/apps/settings_power/power_settings_model.h`
- `src/apps/settings/power/power_settings_model.cpp`
- `tests/apps/settings/power/tst_power_settings_model.cpp`
- `tests/apps/settings/power/tst_power_settings_slider.cpp`

## Acceptance evidence

All commands ran from the assigned worktree. Configure commands used the exact
lane recipe and private KWin 6.6.5 initial cache.

- Debug configure, exact prescribed command under
  `/home/cabewse/work_SPaC3/builds/qindaqt/power-settings-route/debug`: exit 0.
- Release configure, exact prescribed command under
  `/home/cabewse/work_SPaC3/builds/qindaqt/power-settings-route/release`: exit 0.
- Debug focused build of `qindaqt_power_settings_model_tests`,
  `qindaqt_power_settings_slider_tests`, `qindaqt_power_page_tests`,
  `qindaqt_settings_route_registry_test`,
  `qindaqt_settings_navigation_controller_test`,
  `qindaqt_settings_navigation_page_test`, and `qindaqt-settings`: exit 0.
- Release focused build of the same targets: exit 0.
- Debug, with `env -u DBUS_SESSION_BUS_ADDRESS
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`,
  `ctest -R '^qindaqt\.settings-power-' --output-on-failure
  --no-tests=error`: exit 0, **6/6 passed**.
- Release, same poisoned environment and Power selector: exit 0,
  **6/6 passed**.
- Debug, same poisoned environment, Settings Center selector
  `^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$`:
  exit 0, **9/9 passed**.
- Release, same poisoned environment and Settings Center selector: exit 0,
  **9/9 passed**.
- `./tools/validate-docs`: exit 0, **130 Markdown documents plus navigation
  validated**.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/power-settings-route/site`:
  exit 0.
- `./tools/check-source-shape`: exit 0, **2101 source files checked**, with
  only the reported pre-existing non-failing decomposition-review warnings in
  unrelated/shared files.
- `git diff --check`: exit 0.
- No JSON file changed; `python3 -m json.tool` was not applicable.

## Bounded caveats

This candidate claims only injected-fake, offscreen/software-renderer, source-
policy, and relocated-package evidence. It does not contact or qualify a host
session/system bus, UPower, power-profiles-daemon, logind, sysfs, Wayland,
hardware brightness, live AT-SPI, or a nested desktop session. It adds no
session action, internal-display mutation, hold mutation, persistence, or new
public/client/platform authority.

Requested next action: **independent exact review of
`0392aee499e1de3426cd953a61694ed04f1facb9`, then manager integration**.
