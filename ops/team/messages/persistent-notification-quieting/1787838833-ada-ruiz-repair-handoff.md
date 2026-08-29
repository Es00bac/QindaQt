# Ada Ruiz — Settings1 repair candidate handoff

- **Timestamp:** 2026-08-27T13:53:53Z
- **Preserved rejected candidate:** `00b3d49ac3d7ba94edcf10272fa5e61185d63b56`
- **Repair candidate:** `55105b2c565f25f0582303e4936bcd288b04ffdb`
- **Branch/worktree:** `worker/ada-settings1` at
  `/home/cabewse/work_SPaC3/container-wm-workers/ada-settings1`
- **Tree:** clean
- **Repair size:** 36 paths; 2,260 insertions; 420 deletions

## Repair outcome

This one new imperative commit repairs all six Rowan P1 blockers and both P2
issues without amending the preserved candidate:

1. Opaque QtDBus arrays/maps now stream through a shared value and
   snapshot/transaction aggregate node/byte/depth/list/map/key/string budget.
   Children are charged before retention/append/insert, and fixed reply maps
   plus outer reply arity are bounded before semantic validation. Real private-
   bus tests cross node/byte/envelope limits.
2. SettingsClient and Qt transport serialize activation. Failure and successful
   activation without a stable owner use configured backoff; explicit refresh
   cannot stack attempts. Repeated synchronous transport-start failure retains
   honest Unavailable/Retry, and later recovery succeeds.
3. Commit replies require the exact field set and initiating epoch, settings
   schema, base revision, key maps, bounded/deduplicated changes, message, and
   status/revision relationship. Invalidations cannot create target-revision
   loops. Exact-owner relays and pending replies carry captured generations;
   replacement subscription failure publishes owner loss first. Same-owner
   epoch changes and equal-revision contradictions are rejected.
4. Production Settings1 requires and validates the installed
   `profile-defaults/qindaqt.json`, composes it before user overrides, migrates
   valid v1 profiles in memory, persists v1 user migration only after winning
   its name, and rejects missing/wrong-layer/future/corrupt inputs. Real
   snapshot/set/remove proof reports `160` from `profile-defaults` after remove.
5. Initial synchronous start failure reaches controller and both offscreen QML
   surfaces as Unavailable with bounded diagnostics. Retry reattempts start and
   transitions through Loading to Ready; an identical second failure does not
   strand the controller in Loading.
6. A private-bus composition scenario saves and reopens the ordinary controller,
   constructs then reconstructs the shell bridge while the service remains,
   reconstructs the service from the same isolated file, and constructs a new
   shell bridge. Every policy starts fail-quiet, the persisted false value is
   explicitly sourced from `user-overrides`, and service revisions prove no
   consumer replay.
7. Qt transport teardown removes exact-owner and bus-local subscriptions;
   start-ready/stop/start-ready passes on the same object.
8. The normative focused regex includes
   `notification-quieting-controls-offscreen`; architecture, protocol, ADR, and
   test documentation match the repaired behavior.

The profile fallback regression also exposed one repository-global revision
bug beyond Rowan's list: a scoped client that ignored an unrelated-key
invalidation later committed from a stale base. Every valid bounded
invalidation now refreshes the scoped commit base while remaining only a
one-refresh hint.

## Changed paths

- `docs/wiki/adr/0012-persist-notification-quieting-through-settings1.md`
- `docs/wiki/architecture/settings-service.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/settings1-v1.md`
- `src/services/settings_client/include/qindaqt/services/settings_client/{qt_settings_transport.h,settings_client.h,settings_transport.h}`
- `src/services/settings_client/src/{do_not_disturb_controller.cpp,qt_settings_transport.cpp,settings_client.cpp,settings_reply_validation.cpp,settings_reply_validation_p.h}`
- `src/services/settings_protocol/include/qindaqt/services/settings_protocol/{settings_wire_contract.h,settings_wire_decode.h}`
- `src/services/settings_protocol/src/{settings_value_codec.cpp,settings_wire_decode.cpp}`
- `src/services/settings_service/include/qindaqt/services/settings_service/resident_settings_service.h`
- `src/services/settings_service/src/{main.cpp,resident_settings_service.cpp,settings_object.cpp}`
- `src/settings/include/qindaqt/settings/settings_document.h`
- `src/settings/src/{settings_document.cpp,settings_migration.cpp}`
- `tests/apps/settings_center/tst_settingsapp.qml`
- `tests/services/settings_client/{CMakeLists.txt,tst_qt_settings_transport.cpp,tst_qt_settings_transport_adversarial.cpp,tst_settings_client.cpp}`
- `tests/services/settings_protocol/{CMakeLists.txt,tst_settings_protocol.cpp,tst_settings_protocol_dbus.cpp}`
- `tests/services/settings_service/tst_settings_service_lifecycle.cpp`
- `tests/settings/tst_settings_migration.cpp`
- `tests/shell/{CMakeLists.txt,tst_notificationquietingsettingsbridge.cpp}`
- `tests/shell/qml/tst_notificationquietingcontrols.qml`

## Final verification on the exact repair tree

- `cmake --build build/ada-debug -j2` — exit 0.
- `ctest --test-dir build/ada-debug -R '^qindaqt\.' --output-on-failure -j2`
  — exit 0, **68/68 passed**.
- `cmake --build build/ada-release -j2` — exit 0.
- `ctest --test-dir build/ada-release -R '^qindaqt\.' --output-on-failure -j2`
  — exit 0, **68/68 passed**.
- `cmake --build build/ada-production-release -j2` — exit 0, production
  `qindaqt-shell`, `qindaqt-settings`, and `qindaqt-settings-service` built.
- `cmake --build build/ada-production-release --target all_qmllint -j2` —
  exit 0; only the pre-existing unrelated preview-QML warnings were reported.
- `tools/validate-docs` — exit 0, **42 documents**.
- `build/ada-mkdocs-venv/bin/mkdocs build --strict` — exit 0.
- `tools/check-source-shape --largest 20` — exit 0, **760 files**, zero
  allowlist skips. Largest repaired production files remain below the
  500-nonblank decomposition threshold: SettingsClient 490, wire decoder 482.
- `git diff --check`, cached diff check, and final clean-tree check — exit 0.
- Production Release staged into isolated `build/ada-stage-prefix`; service,
  shell, settings app, activation descriptor, schemas, and QindaQt profile
  default are present — exit 0.
- Isolated `dbus-run-session` activation through the staged descriptor — exit
  0. `GetSnapshot` returned Applied, wire schema 1, settings schema 2, revision
  0, `appearance.animationDurationMs=160` from `profile-defaults`, and
  `services.doNotDisturb=false` from `system-defaults`.
- A final delegated read-only static scan reported no remaining P1/P2 source or
  evidence finding and did not claim build/test execution.

## Caveats and requested next action

No live desktop, real user session bus, compositor, assistive-technology bridge,
lock screen, KGlobalAccel, uinput, pointer, or keyboard automation was used.
Private D-Bus and offscreen QML are the intentional safe boundary.

Please assign a different worker to re-review exact repair candidate
`55105b2c565f25f0582303e4936bcd288b04ffdb` against every item in Rowan's
complete rejection `1787836191-rowan-ivers-review-complete.md`, including the
follow-up generation/activation/retry/epoch/outer-arity/reconstruction cases.
Only that exact-commit review should decide integration acceptance.
