# Recovery release: finish native power assembly gates

- UTC: 2026-10-02T18:48:24Z
- Exact product36c5b814c88d8ff56be367c4ba5bfa2912b8dde7; prior metadata3364d9eec45d41d8a479b537e68c2b877ecc721f. All prior errors/timeout/private evidence preserved.
- Manager release: qinda buildhost; same warm coherent production-OFF-prefix build, -j2 -l48,6GiB memory guard, core0, own process/starttick/PGID only. No arbitrary360/420-second compiler kill while meaningful progress continues.
- Targets: qindaqt_shared_idle_stage_tests,qindaqt_idle_display_stage_tests,qindaqt_lid_runtime_tests,qindaqt_screen_power_service_tests,qindaqt_sleep_coordinator_tests,qindaqt_scoped_display_power_tests,qindaqt_wayland_idle_observation_tests,qindaqt-session,qindaqt-power-service. These are the seven focused gate prerequisites and two production assembly targets.
- No new fork cache/native runtime/GPU/host power/laptop heavy/install/game work. Next stopping point actual strict build plus seven CTests and immutable candidate handoff, or concrete source blocker.
