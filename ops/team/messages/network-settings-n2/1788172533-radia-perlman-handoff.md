# Radia Perlman — Network Settings N2 exact candidate handoff

- Timestamp: 2026-08-31T04:35:33-06:00
- Candidate: `43b563cdfe08455269375e2c356112902e562641`
- Tree: `31ec5f7acb312bd2f53884351fc6fdaa1b84dd5f`
- Sole parent/base: `9b3d65542c87b2b977482ed7e72e4425c5332dd6`
- Branch: `worker/network-settings-n2`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/network-settings-n2`
- Worktree state: clean

## Outcome

Adds the production `network` Settings route over the already integrated
public Network1 Qt transport/client boundary. The process-lifetime route model
projects exact owner/epoch/revision and bounded radio/device/access-point/saved-
network truth, preserves only explicit stale read-only truth, clears retired
owner inventory, and never manufactures state from operation replies. It
exposes only reload, admitted scan, existing-saved-network connect, and active-
device disconnect intents. There is no radio, profile, credential, secret-
agent, libnm, private-service, direct-D-Bus, host-network, persistence, shell,
or session-runtime authority.

The canonical route, Ctrl+4 navigation, responsive QST/Controls presentation,
keyboard/focus/accessibility behavior, installed module proof, five poison
controls, ADR-0055, and the primary Network Settings wiki page are included.

## Changed paths

- New route: `src/apps/settings/network/CMakeLists.txt`, public model header and
  implementation, and the five `qml/Network*.qml` files.
- Additive Settings/build seams: `src/CMakeLists.txt`,
  `src/apps/settings_center/{CMakeLists.txt,Main.qml,SettingsRouteHost.qml,main.cpp,settings_route.cpp,settings_route.h,settings_route_registry.cpp}`.
- New focused tests: `tests/apps/settings/network/{CMakeLists.txt,check_boundary.cmake,check_boundary_negative.cmake,network_settings_test_support.h,stub_network_settings_model.h,tst_network_page.cpp,tst_network_settings_model.cpp,tst_network_settings_model_adversarial.cpp}`.
- Additive Settings test/package seams: `tests/CMakeLists.txt` and
  `tests/apps/settings_center/{CMakeLists.txt,check_installed_routes.cmake,check_route_construction.cmake,tst_settings_navigation_controller.cpp,tst_settings_navigation_page.cpp,tst_settings_route_registry.cpp}`.
- Normative documentation/navigation: `docs/wiki/adr/0055-compose-network-settings-through-network1.md`,
  `docs/wiki/adr/index.md`, `docs/wiki/apps/network-settings.md`,
  `docs/wiki/apps/settings-center.md`, `docs/wiki/architecture/network-service.md`,
  `docs/wiki/index.md`, and `mkdocs.yml`.

No path under `tests/session/**`, Display implementation/tests, shell,
features/HANDOFF/TASK_LIST/queues, or private Network platform modules changed.

## Exact candidate evidence

Strict Debug and strict Release were freshly configured with
`QINDAQT_ENABLE_STRICT_WARNINGS=ON`, shell/production-shell/KWin-plugin disabled,
testing enabled, and the pinned KDecoration prefix. All requested production
and focused test targets compiled without warnings.

The exact committed tree then ran this selector in both build roots:

```sh
ctest --test-dir /tmp/qindaqt-radia-network-{debug,release} \
  --output-on-failure --no-tests=error --parallel 1 \
  -R '^qindaqt\.(network-(settings-model|settings-model-adversarial|page|settings-boundary|settings-boundary-poison)|settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$'
```

- Debug: exit 0, 14/14 passed.
- Release: exit 0, 14/14 passed.
- The package row stages only `SettingsAppearanceRuntime`, withholds installed
  Appearance and Network QML modules independently while their developer trees
  remain, requires exit 3, restores the package, and proves all four routes
  from the relocated prefix.
- The Network boundary poison row rejects private service inclusion, direct
  Qt D-Bus, a radio invokable, credential text input, and private service
  linkage.

Static gates on the frozen content:

- `tools/validate-docs`: exit 0, 112 Markdown documents and MkDocs navigation.
- `/tmp/qindaqt-display-repair2-docs-venv/bin/mkdocs build --strict`: exit 0.
- `tools/check-source-shape`: exit 0; only three unrelated pre-existing
  warnings, and the largest new production source is 461 nonblank lines.
- `git diff --cached --check` before commit and clean-tree/base/provenance checks:
  exit 0.

## Bounded caveats and requested action

Evidence is deterministic fake-public-transport, absent-private-bus, offscreen,
and relocated-package coverage. It deliberately does not claim physical Wi-Fi,
Ethernet/radio behavior, host NetworkManager mutation, stored-profile
compatibility, credential entry, external secret-agent qualification,
persistence, shell applets, or session-runtime integration. Secured saved-
network activation can only rely on an independently registered external
NetworkManager secret agent and fails closed with an explanatory error.

Please assign a different worker to review exact commit
`43b563cdfe08455269375e2c356112902e562641`, reporting P0/P1/P2/P3 findings and
rerunning at least the focused Network/Settings selector plus boundary/package
proof. Return any blocking reproduction to this worktree; otherwise route the
accepted exact candidate to manager integration.
