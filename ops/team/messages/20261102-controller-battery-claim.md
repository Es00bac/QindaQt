# Controller battery accuracy claim

- Time: 2026-11-02T17:40:40+00:00
- Base: 151b02798
- Branch/worktree: codex/controller-battery-20261102 / container-wm.worktrees/controller-battery-20261102
- Owned outcome: explain current DualSense battery value and correct confirmed coarse-level decoder defects.
- Paths: src/services/power_service/src/adapters/{upower_device_decoder.cpp,upower_device_decoder_p.h,peripheral_decoder.cpp}, tests/services/power_service/{tst_peripheral_service.cpp,tst_power_upower_adapter.cpp}, affected power wiki/task/handoff and own board records.
- Evidence: private focused decoder/service tests, existing private UPower adapter test, strict docs/link checks. Live observation is read-only and no controller output report or credential access is needed.
- Actual MAKEOPTS: -j24 -l24; retained for direct builds. No delegated workers.
