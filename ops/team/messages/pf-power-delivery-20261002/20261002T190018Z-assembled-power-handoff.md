# Native shared power: exact assembled candidate handoff

- Product/test candidate: `d1b228be96ab7cd5a5e3c549ec3d44113056c045` at base `7358b792874f06eb13e00dcd0780c1ee8a453e7a`; branch worker/pf-power-delivery-20261002 pushed to qinda hub.
- Companion fork: `26cf50cdfa5404f1a9d41674a8a956be57054b6b`, base `68c4d74f903b7e8990dd5fd5d509ec8154eac1d1`, branch worker/pf-display-power-authority-20261002 preserved in qinda hub.
- Exact relative-path/hash/artifact evidence: [20261002T190018Z-assembled-source-artifact-inventory.json](20261002T190018Z-assembled-source-artifact-inventory.json). No source change follows actual gates; this handoff adds own ops only.

## Delivered source behavior

Native composition now assembles actual authenticated dim/display/idle-suspend consumers, current Settings source/profile lineage, genuine activity rearming, consumed-scope registration and conservative inhibitor receipts. Scoped compositor display causes preserve preexisting and external Off, keep idle/lid causes independent, and never perform unconditional all-On restoration. Persistent lid ScreenOff uses a current Power1 owner/epoch authenticated ScreenPower1 facade and the existing Protected lock gate; existing SleepCoordinator/logind privacy flow remains authoritative.

The explicit QINDAQT_NATIVE_POWER_EXCLUSIVE option defaults OFF. ON configures all four native-exclusive Power1 flags and supervisor native-power exclusive composition, suppressing only its owned PowerDevil child. Bare service remains default off. This is concrete source for package cutover; nothing is installed or enabled on the user session.

## Actual executable gates

Same coherent qinda Debug/shared/testingON/strictON/pluginOFF/uinputOFF/ccache warm cache, exact68c production AUTHOFF prefix and libexec. Actual native AUTHON core was not used as the consumer prefix. Strict assembled command:

```text
cmake --build build/dev --target qindaqt_shared_idle_stage_tests qindaqt_idle_display_stage_tests qindaqt_lid_runtime_tests qindaqt_screen_power_service_tests qindaqt_sleep_coordinator_tests qindaqt_scoped_display_power_tests qindaqt_wayland_idle_observation_tests qindaqt-session qindaqt-power-service -- -j2 -l48
```

Exit0/100.904s, minimum13,191,440KiB available, core0. The first seven-test run exit8/40.412s found the old unsupported ScreenOff negative row now had the supported private facade. Exact failed CTest/Qt/receipt archives remain under ignored build/evidence, hashes in inventory. Only that negative fixture row was corrected to withdraw private ScreenPower1 before resident start, preserving both no-action/no-owned-FD assertions and all positive ScreenOff/restoration rows. No production bytes changed after36c5. Affected target strict incremental rebuild exit0/19.649s, minimum11,994,196KiB available.

```text
ctest --test-dir build/dev --output-on-failure -R ^qindaqt\.(shared-idle-stages|session-idle-display-stage|power-lid-runtime|screen-power-service|sleep_coordinator|scoped-display-power|wayland-idle-observation)$
```

**7/7 CTest;104 Qt PASS/0FAIL/0SKIP**, exit0/40.368s. Exact Qt counts in run order: Lid38, SleepCoordinator17, DisplayOff11, SharedIdle8, ScopedDisplay11, ScreenPower6, WaylandIdle13. The independent-cause, Protected-first, wrong-owner/epoch, canceled-late-reply, inhibitor/readiness, no-replay and external-brightness-value assertions passed. All tests use private buses/fake hardware facts/dead ambient buses/no display/offscreen; no physical host action or GPU test.

Final strict MkDocs exit0/9.60s; python3 tools/validate-docs validates492 Markdown documents plus navigation exit0/0.60s. git diff --check passes. Earlier authenticated-admission slice12CTests/136Qt and strict pure fork ledger7Qt passed, receipts retained; those are separate prior component evidence, not new production fork acceptance.

Final ignored complete receipt SHA256 `65550e5dda9f527e38358a07e0f6e06ec04dd1dd5641b96cc80b5c111c950913`. Final Qt log SHA256 `79343821cef307c7486e25f3068ea01a6e101a8c01ec77f5a20c9f940a3b0df9`. PIDs/startticks: assembled compile1521532/42180387 (88 observed members), fixture compile1539147/42229218 (15), final test1540463/42233417 (63). All leaders/groups and166 observed same-starttick members gone;83 logged private fixture/service PIDs gone; no owned build core files. Source tree clean at receipt. Compiler released immediately after tiny target; no further runtime/compiler claimed.

## Bounded remaining acceptance

Different-worker exact source/CPU retest required before manager integration. Fork Workspace adapter still needs root-coordinated exact26cf composition onto the existing native-input warm source/cache and strict production compilation, followed by actual private native output/DPMS/inventory/wake/external-Off and supervisor activation qualification. No physical hardware or package cutover is claimed. Current full branch therefore remains development-default-off.

Brightness restoration still has the explicitly accepted bounded limitation: existing Power1 brightness operations lack atomic expected-value/revision compare; an external write racing stale readback cannot be completely excluded. Existing different-value preservation tests pass. No lease framework or unrelated protocol was added.

Requested next action: independently review/retest this exact candidate, then manager integrate and execute the coherent fork/native/package gates. Compatible help offered: source-only repair of exact review findings or prepare narrowly authorized native-composition fixture expectations; no further gate starts without coordination.

## Changed product paths

- docs/wiki/adr/0333-authenticate-shared-idle-consumer-registration.md
- docs/wiki/adr/0338-own-scoped-display-power-and-shared-idle-composition.md
- docs/wiki/adr/index.md
- docs/wiki/architecture/idle-policy.md
- docs/wiki/architecture/power-service.md
- docs/wiki/reference/power1-v1.md
- mkdocs.yml
- src/CMakeLists.txt
- src/platform/idle_observation/include/qindaqt/platform/idle_observation/idle_observation.h
- src/platform/idle_observation/src/wayland_idle_observation.cpp
- src/services/power_client/CMakeLists.txt
- src/services/power_client/include/qindaqt/services/power_client/idle_consumer_registrar.h
- src/services/power_client/src/idle_consumer_registrar.cpp
- src/services/power_service/CMakeLists.txt
- src/services/power_service/app/main.cpp
- src/services/power_service/data/org.qindaqt.Power1.service.in
- src/services/power_service/data/org.qindaqt.Power1.xml
- src/services/power_service/data/qindaqt-power-service.service.in
- src/services/power_service/include/qindaqt/services/power_service/resident_power_service.h
- src/services/power_service/src/idle_consumer_authority.cpp
- src/services/power_service/src/idle_consumer_authority_p.h
- src/services/power_service/src/lid_policy.cpp
- src/services/power_service/src/power_idle_consumer_registration.cpp
- src/services/power_service/src/power_service_object.cpp
- src/services/power_service/src/power_service_object_p.h
- src/services/power_service/src/resident_power_service.cpp
- src/services/session_actions/CMakeLists.txt
- src/services/session_actions/include/qindaqt/services/session_actions/session_actions_client.h
- src/services/session_actions/src/session_actions_availability.cpp
- src/services/session_actions/src/session_actions_client.cpp
- src/services/session_actions/src/session_actions_screen_off.cpp
- src/services/session_actions/src/session_actions_state.cpp
- src/session/display_power/CMakeLists.txt
- src/session/display_power/include/qindaqt/session/display_power/display_power_facade.h
- src/session/display_power/include/qindaqt/session/display_power/scoped_display_power.h
- src/session/display_power/include/qindaqt/session/display_power/screen_power_service.h
- src/session/display_power/org.qindaqt.ScreenPower1.xml
- src/session/display_power/src/display_power_facade.cpp
- src/session/display_power/src/scoped_display_power.cpp
- src/session/display_power/src/screen_power_service.cpp
- src/session/idle_policy/CMakeLists.txt
- src/session/idle_policy/include/qindaqt/session/idle_policy/dim_stage.h
- src/session/idle_policy/include/qindaqt/session/idle_policy/display_off_stage.h
- src/session/idle_policy/include/qindaqt/session/idle_policy/idle_suspend_stage.h
- src/session/idle_policy/include/qindaqt/session/idle_policy/shared_preferences.h
- src/session/idle_policy/src/dim_stage.cpp
- src/session/idle_policy/src/display_off_stage.cpp
- src/session/idle_policy/src/idle_suspend_stage.cpp
- src/session/idle_policy/src/shared_preferences.cpp
- src/session/idle_policy/src/source_preferences.cpp
- src/session/native_sleep/include/qindaqt/session/native_sleep/sleep_coordinator.h
- src/session/native_sleep/src/sleep_coordinator.cpp
- src/session_supervisor/CMakeLists.txt
- src/session_supervisor/app/main.cpp
- src/session_supervisor/src/native_lock_composition.cpp
- src/session_supervisor/src/native_lock_composition.h
- src/session_supervisor/src/native_power_composition.cpp
- src/session_supervisor/src/native_power_composition.h
- src/session_supervisor/src/native_sleep_composition.cpp
- src/session_supervisor/src/native_sleep_composition.h
- tests/CMakeLists.txt
- tests/platform/idle_observation/tst_wayland_idle_observation.cpp
- tests/services/power_service/CMakeLists.txt
- tests/services/power_service/support/lid_session_wire.h
- tests/services/power_service/tst_idle_consumer_registration.cpp
- tests/services/power_service/tst_lid_runtime.cpp
- tests/session/display_power/CMakeLists.txt
- tests/session/display_power/tst_scoped_display_power.cpp
- tests/session/display_power/tst_screen_power_service.cpp
- tests/session/idle_policy/CMakeLists.txt
- tests/session/idle_policy/tst_display_off_stage.cpp
- tests/session/idle_policy/tst_shared_idle_stages.cpp
- tests/session/native_sleep/tst_sleep_coordinator.cpp

Fork paths are listed with exact hashes in the same inventory; own ops board/thread files are the only additional paths.
