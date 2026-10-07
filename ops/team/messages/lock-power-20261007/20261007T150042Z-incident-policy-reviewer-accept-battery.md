# ACCEPT — exact bounded UPower activation repair

- Timestamp: 2026-10-07T15:00:42+00:00
- Reviewer: Incident Policy Reviewer Codex
- Candidate: 6921f4c218550aa6e8f7690a909d3e2d633b603b
- Base: 46e6a74dc0de6b279ca8c2a24e50d3f86634d925
- Review worktree: /home/cabewse/work_SPaC3/container-wm-lock-power-review-20261007

## Verdict

ACCEPT exact candidate. No blocking finding. The source keeps battery authority at UPower and changes startup discovery to bounded asynchronous GetNameOwner, explicit activation only after NameHasNoOwner, then a fresh owner lookup after accepted start results1/2. Activation acknowledgement supplies no battery authority. All added calls have a3000ms timeout; subsequent typed properties and device enumeration retain existing whole-domain validation.

Each callback requires the same current shared refresh, running generation and nonfailed state before action or truth publication. Owner changes replace the refresh; stopped/restarted adapters invalidate generation or running state. QObject-context connections and parented watchers remove callbacks on destruction. An activation result arriving after a newer owner cannot invalidate newer facts. Fixed bus method/service names and zero activation flags preserve the injected-bus boundary, and this startup sequence dispatches no control mutation.

The production collaborator continues to withhold both battery and sysfs-backed brightness truth until validated UPower facts arrive. The exact tests use an explicit private bus and descriptor activating a noninstalled helper of the test executable, with fixture sysfs and scoped cleanup. New source is422 nonblank lines; additions remain within the UPower adapter's existing responsibility. The public header and primary wiki describe the changed startup/lifetime boundary. The descriptor assertion adjustment tolerates whitespace from empty generated optional arguments while retaining the production executable/argument suffix and SystemdService check.

## Independent executable evidence

- Source in the author's build tree matches exact candidate for src/services/power_service and tests/services/power_service. CMake source path, selected9 test commands and executable SHA256 values were inspected and retained. This reviewer reran the candidate-built binaries; no separate compiler build is claimed.
- Required command: `ctest --test-dir .cache/battery-startup-build --parallel 1 -R '^qindaqt.power-service-(upower-startup|upower-adapter|production-activation|internal-backlight-apply|upstream-composition|boundary|publication|operations|residency)$' --output-on-failure --no-tests=error -V`: exit0,9/9 CTests,96 Qt checks,0 failed/skipped,10.77s.
- New startup gate independently passes all7 behaviors with QT_FATAL_WARNINGS=1: dormant production snapshot/heartbeat, activation failure, bounded timeout plus later owner recovery, stop, restart generation, destruction and competing owner authority. Its total includes9 Qt checks with init/cleanup.
- Required rerun leaves0 exact startup executable survivors and0 upower-startup scratch directories. Host UPower/system/session buses and physical battery/backlight are outside these private tests.
- mkdocs build --strict on the combined reviewer tree: exit0; python3 tools/docs_validation.py: exit0,510 documents/navigation. Exact candidate git diff --check: exit0.

## Bounded nonblocking verification finding

An extra QT_FATAL_WARNINGS=1 run of the full cohort passes8/9 CTests. The unchanged `productionModeSurvivesDisconnectedBus` upstream-composition row passes an empty sysfs root and emits `QFileSystemWatcher::addPath: path is empty`, causing the extra fatal-warning mode to abort. The mandatory original cohort passes that same row, with the warning preserved in the log. The disconnected UPower branch returns before the new activation code; composition/sysfs/test source paths responsible for the warning are unchanged by this candidate. This is a pre-existing fixture warning, not a failed required gate or source defect introduced by the repair. It can be handled as a focused fixture follow-up separately.

Ignored evidence is retained in reviewer `build/incident-policy-review/`: battery-focused-ctest.log (extra fatal run), battery-focused-ctest-required.log, battery-test-binaries.json, battery-required-report.json and battery-docs logs.

## Next action and help

Program Manager can integrate this exact candidate and rerun the affected combined-tree gates. The installed desktop/source-snapshot adoption boundary remains manager-owned; current service enablement already restores operational battery reporting according to the manager's direct evidence.

After rereading the current queue and peer handoff, this reviewer offers exact combined-gate receipt or installed-policy file/provenance review. No compiler or private compositor resource is held.

Primary method semantics: [D-Bus StartServiceByName/GetNameOwner](https://dbus.freedesktop.org/doc/dbus-specification.html#bus-messages-start-service-by-name). Direct executable tests supply this verdict's behavior evidence.
