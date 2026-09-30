# Power1 lease methods checkpoint — Codex native lock recovery

Exact source checkpoint: `7c5dd751f012db3a0aa828ee68290f1bdc7ec3d9` on `feature/native-lock-runtime`, pushed to qinda's local hub. It follows the tested private registry core `a78b96d5fbc2d8dc395d400f4bfa8ba2e81a2e32`.

Changed paths:
- `src/services/power_service/src/power_service_object.cpp`
- `src/services/power_service/src/power_service_object_p.h`
- `src/services/power_service/src/idle_inhibitor_registry.cpp`
- `src/services/power_service/src/idle_inhibitor_registry_p.h`
- `src/services/power_service/data/org.qindaqt.Power1.xml`
- `tests/services/power_service/tst_power_service_residency.cpp`
- `docs/wiki/reference/power1-v1.md`
- `docs/wiki/architecture/power-service.md`

Verification on qinda:
- Read `MAKEOPTS=-j24 -l24`; the residency-target build used the shared exclusive qinda build lock, CMake parallel 24 and Make load limit 24, and exited 0.
- CTest Power1 registry, publication, operations, and private-bus residency (including fail-closed idle methods and service introspection): 4/4 passed.
- CTest Power1 boundary: 1/1 passed.
- `python3 tools/validate-docs`: exit 0, 458 Markdown pages/navigation.
- `mkdocs build --strict`: exit 0.

The methods are additive D-Bus members while the existing v1 snapshot/mutation signatures remain unchanged. The registry rejects all acquisition requests because no idle scope is yet consumed; capabilities and active masks are zero, and no compatibility call can report success. Unique D-Bus sender identity is taken from the actual incoming method call; epoch replacement clears leases and owner watches.

Remaining scope: public PowerClient APIs, actual automatic lock/display-off/idle-suspend consumers, ScreenSaver facade routing to Power1, and a production owned-suspend path through the Protected receipt. External privileged logind actions cannot be universally denied. No live host action or installation was used.

Next action: continue on this branch with the public client and production idle-stage consumer, keeping every scope unavailable until its caller is consumed.
