# Bounded native power startup gate PASS / real core gate pending

- Timestamp:2026-10-02T19:57:30Z
- Exact tested candidate:818b1a3242ffecba7b558ce5ff26a07c51193db4, qinda hub worker/pf-power-exclusive-startup-repair-20261002.
- Production source385edc853691d77e24d7e97b6e120abfeaf5e0b0 independently source-reviewed by root with no blocker;818 changes only own fixture/CMake isolation and primary verification note.
- Exact fork constant repair:2b09fd27025328c345fcc37be9cb14db2f8f618c, declaration+2uses SessionService only. Parent preserves actual127s core failure; root owns coherent core rebuild and fixture10c compilation/runtime.

## Actual commands and outcomes

Warm own Power cache, strict Debug/pluginOFF/PowerExclusiveOFF, unchanged old68
productionAUTHOFF prefix, no ALL/install/GPU/physical actions. Core0,6GiB memory
guard, same-starttick owned PGIDs; source hashes frozen13 paths/unchanged.

1. `cmake -S . -B build/dev`: exit0/45.207s,PID1663373/starttick42572602.
2. `cmake --build build/dev --target qindaqt_native_power_startup_tests -- -j2 -l24`: exit0/14.411s,PID1665192/starttick42577123 (test plus necessary session/support/probe/token helper dependencies only).
3. `ctest --test-dir build/dev --output-on-failure -R ^qindaqt.native-power-startup$`: exit0,1/1CTest/0fail,0.26s (outer guard1.052s),PID1666006/starttick42578564. Actual Qt4PASS/0FAIL/0SKIP/218ms.
4. `mkdocs build --strict --site-dir build/docs-native-power-startup`: exit0/8.075s,PID1667200/starttick42583411.
5. `python3 tools/validate-docs`: exit0/0.464s,501Markdown documents/navigation,PID1667366/starttick42584219.

Task-only EXTERNAL-auth DBus config contains no service dirs/includes. Actual
exclusive CLI rejects missing compositor with exit2 and no PowerDevil probe.
Off mode starts the real owned probe, ordinary parent Logout is Unauthorized,
then the existing actual supervised shell helper performs admitted CanLogout/
Logout. Owned probe1666028/starttick42578571 is absent after normal session exit0.
All24 observed compile/test same-starttick members gone, three groups and two
doc leaders gone, no own archive cores. Do not infer unobserved process counts.
No service activation message appears in repaired actual log.

## Preserved first failure

385 configure0/49.306s and strict build0/41.152s passed. Its first actual CLI
CTest exited8:3QtPASS/1FAIL/0SKIP; parent Logout error type incorrectly expected
success. The log did not include exact error name, so later source diagnosis is
kept separate: Session1 intentionally authenticates actual supervised shell PID,
and marker creation could precede Session1 registration. Standard private DBus
also activated installed Settings1; that failed fixture is not isolation or
acceptance evidence. Root granted only actual-shell cleanup/no-activation config
repair plus one replay. All first logs/hashes remain untouched;90 observed
same-starttick first-gate members were gone. No healthy repetition occurred.

## Boundary / next gate

This proves only fail-closed exclusive absent-compositor startup and compatible
owned-child lifecycle. It does not exercise supported complete NativePower
startup or the asynchronous6s readiness path. Real DisplayPower/Workspace
fixture10c remains source-only pending root coherent core build and actual
private nested run. Supported composition/shared inhibition/lid/source/hardware/
package cutover are not claimed. Full receipts, commands, hashes, observed
process/startticks and five executable/support artifact hashes are in the paired
immutable JSON. Production doc changes and exact fixtures require review of818;
root then integrates/retests. Compiler and private CPU-bus leases released;
worker available for next bounded real native qualification/repair. No host
session/display/power action or game work.
