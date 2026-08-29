# Mara Voss — P1 design midpoint

- Time: 2026-08-28T13:38:30Z
- Owner: Mara Voss
- State: working; design locked, source construction starting

Design decisions for `src/shell/power_applet` (new paths only):

- Pure Qt Core presentation values + free projection functions in namespace
  `QindaQt::Shell::PowerApplet`; no QObject, no QML, no transport, no clocks.
- `projectPowerApplet(Power::Snapshot, powerOwnerAvailable, BrightnessView)`
  fails closed on owner loss, `wireValid=false`, `Starting`/`Unavailable`
  phases, hostile numbers (NaN/inf/out-of-range fall back to unknown), and
  out-of-vocabulary enum raws (range-checked before cast; no UB switches).
- Time remaining is presented only when the known flag is set AND the charge
  state direction matches AND the value is within the Power1 bound; never
  derived from rate/energy.
- Severity maps only upstream truth (warning levels and coarse battery
  levels); no invented percentage thresholds.
- Display/keyboard control rows come from an injected composed
  `Brightness::ModelSnapshot` when its owner is present; otherwise snapshot
  fallback rows are identity-visible but Unavailable and non-adjustable.
- `BrightnessRequest` is a bounded pure request state machine over
  `OperationResult`: pending holds initiating lineage; stale-lineage replies
  are discarded without completing; success requires observed lineage at or
  after initiating; owner/epoch loss yields typed Uncertain; terminal
  requests never replay.
- New unwired `CMakeLists.txt` in my owned dirs plus a `check_boundary.cmake`
  static gate I can run without configure. Future additive seams (src/tests
  CMake `add_subdirectory`, QML registry, manifest catalog, module-boundaries
  row, testing-harness matrix rows) go in the handoff for the manager.
- I will not touch the four declared PB-0 integration-conflict paths
  (module-boundaries.md, testing-harness.md, src/CMakeLists.txt,
  tests/CMakeLists.txt) so the manager rehearsal stays exact.
