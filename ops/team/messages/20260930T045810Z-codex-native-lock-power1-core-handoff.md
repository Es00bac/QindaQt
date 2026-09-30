# Power1 idle lease core checkpoint — Codex native lock recovery

Exact checkpoint: `a78b96d5fbc2d8dc395d400f4bfa8ba2e81a2e32` on `feature/native-lock-runtime`, pushed to qinda's local hub `/home/cabewse/git/container-wm.git`. It is based on PF8/runtime/docs commits `c701f8e5`, `e299511d`, and ADR-0306 commit `0faeebcd`.

Changed paths:
- `src/services/power_service/src/idle_inhibitor_registry_p.h`
- `src/services/power_service/src/idle_inhibitor_registry.cpp`
- `src/services/power_service/CMakeLists.txt`
- `tests/services/power_service/tst_power_idle_inhibitor_registry.cpp`
- `tests/services/power_service/CMakeLists.txt`
- `docs/wiki/architecture/power-service.md`

Evidence on qinda:
- `portageq envvar MAKEOPTS` returned `-j24 -l24`; all direct builds used the exclusive `/home/cabewse/.cache/qindaqt-program-build.lock`, CMake parallel 24 and Make load limit 24.
- Built `qindaqt_power_idle_inhibitor_registry_tests`, `qindaqt_power_service_publication_tests`, `qindaqt_power_service_operation_tests`, and `qindaqt_power_service_residency_tests`: exit 0.
- CTest `qindaqt.power-idle-inhibitor-registry`, `qindaqt.power-service-publication`, `qindaqt.power-service-operations`, `qindaqt.power-service-residency`: 4/4 passed. The registry executable covers atomic unsupported-scope rejection, owner-bound release, owner-loss and epoch cleanup, input validation, and both capacity bounds.
- `python3 tools/validate-docs`: exit 0, 458 Markdown pages/navigation; `mkdocs build --strict`: exit 0.

Bounded caveat: this is only the tested private registry core. It is not wired into the resident Power1 object or its D-Bus protocol. Its consumed-scope mask is empty by default, so it cannot grant any lease; ScreenSaver Inhibit remains Unsupported, and no stage capability is advertised. Actual lock/display-off/idle-suspend consumers, ScreenSaver facade routing, and a production protected-gated owned-suspend callsite remain open PF2/PF3 work. No live host lock/suspend, secrets, installation, or external logind denial is claimed.

Requested next action: root review/integration of this intermediate source slice if suitable; continue implementation on the same candidate branch by wiring the Power1 lease boundary and real idle-stage consumers, then update the docs and only enable capability after all requested scopes are consumed.
