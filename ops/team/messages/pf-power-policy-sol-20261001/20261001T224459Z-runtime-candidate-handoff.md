# Native source-profile runtime candidate and bounded help offer

- Exact candidate: `0c326be58a61810e33913a2daac71c2921541a78` on `worker/pf-power-policy-runtime-20261001`, base `4011a663`; preserved in qinda hub.
- Executable tested source: `faa71a16e96a5436762bf647d3cccbe956758a56`; candidate differs only in this worker's board evidence. No source/test/docs byte changes after gates.
- Ownership: Power1 narrow profile policy/authority collaborators, actual resident composition, existing profile-hold adapter/public signals, focused tests, primary power wiki and authorized ADR0330 navigation. No supervisor, Settings schema or protocol change.
- Next action: assign a different worker to review the exact candidate, then manager integrate and rerun affected gates. No integration or wholePF2 claim.
- Resources: compiler/private runtime both released. No host bus/profile/settings/hardware mutation or software installation; no laptop heavy build.

## Result

Actual `qindaqt-power-service --profile-policy=native-exclusive` composes public SettingsClient and validated Power1 source facts. The current source chooses confirmed power-saver/performance preferences through exactly one nonce-tagged owned hold. Equivalent facts do not churn; `none` and balanced release only our hold, with balanced explicitly deferred because upstream HoldProfile cannot hold it. The canonical PowerDevil-name guard subscribes before current-owner query, closes on uncertainty/arrival and withdraws the own hold. Default remains off. Manual targeted ProfileReleased suppresses reacquisition until changed source/preference/authority. Known refusals require material admission changes or local explicit retry; uncertain acquisition quarantines new acquisition, uncertain release cannot replay that handle. Public authority/revision/epoch loss cannot select retained stale preferences.

## Executable evidence

All status files/logs remain under the own qinda `build/pf-source-profile-evidence/` directory. Existing failures are retained; no identical failing reruns were used as acceptance.

- Strict Debug/sharedON/testingON/pluginOFF/strictON/host-uinputOFF configure on accepted fork690 stage: exit0. Four focused targets and declared dependencies built with `cmake --build build/dev --target qindaqt_source_profile_runtime_tests qindaqt_power_profiles_adapter_tests qindaqt_power_service_operation_tests qindaqt_power_service_publication_tests -- -j8 -l24`: final exit0. Each later fixture repair rebuilt only the source-profile runtime target/deps. Last exactfaa compile PID/PGID364456 exit0. Core0, unchanged global MAKEOPTS, minMemAvailable3GiB/30s monitor terminating only ownedPGID.
- Changed row `qindaqt_source_profile_runtime_tests ownerLossAndUnsupportedProfiles`: exit0;3 Qt passed/0 failed/0 skipped;971ms (`tests-settings-owner-repair-single.log`). Fresh unique Settings owner after restart is required by the public client contract.
- `ctest --test-dir build/dev -V -R '^qindaqt[.]power-(source-profile-runtime|service-profiles-adapter|service-publication|service-operations|service-boundary)$' --output-on-failure -j1`: exit0;5/5 CTests;65 Qt passed/0 failed/0 skipped, including13 actual runtime behaviors (15 Qt cases), existing publication17, operations17, profile adapter16, and static boundary;13.91s (`tests-repaired-final.log`).
- Real resident subprocess + injected private UPower/PPD + resident Settings1; adversarial Settings1 revision-regression fixture uses the public wire. Explicit untyped D-Bus config has no host includes/service directories, SHA256 `9aa7bf317b1fc05b7ef605a47a7e300a0ffaab0bf7cb48c871c8f7c4a9f3a29b`. Ambient system/session addresses point to dead private paths; HOME/XDG roots are temporary, DISPLAY/Wayland/starter env absent, core0.
- Runtime rows cover AC→battery→low→AC, none/own-only cleanup, repeated-fact stability, external holds, dormant default, legacy authority arrival/loss, Settings owner/revision loss, supported-profile/provider/source loss, hold limit, known refusal, acquisition/release timeout and dispatched provider loss no-replay, balanced deferral, targeted manual cancellation. All still assert mandatory actual service startup.
- Five executable hashes unchanged before/after. Runtime test SHA256 `fb2aef05a4192c670ab46cad9162bd580f782de3faa9bb2df198b40c55c23d87`; Power resident SHA256 `9d56f19dfca2bdb51d72cc088f280e201294d0fd7d829f4e4ca448f33f338534`. All42 logged private process PIDs plus2 runners and28 logged private roots directly absent afterward (`repaired-lifetime-audit.json`). Complete hash inventory retained.
- `tools/validate-docs`: exit0;485 Markdown documents/navigation, links/path validation. `mkdocs build --strict --site-dir build/pf-source-profile-evidence/site`: exit0. `cmake -DSOURCE_ROOT="$PWD" -P tests/services/power_service/check_boundary.cmake`: exit0. `tools/check-source-shape --root src/services/power_service`: exit0,44files, largest415. Test shape: exit0,30files; preexisting unrelated518-line activation-test decomposition warning retained. `git diff --check`: exit0. Local `static-handoff-status.json` records commands/exits.

## Retained failed evidence

Initial nonexistent target spelling and macro-substitution compile failures were repaired before successful builds. First runtime13 rows failed fake startup; splitting independent PPD/UPower unique owners repaired authentic setup. Next source sequence expected churn for equivalent power-saver preferences; using battery none makes the boundary observable without weakening equivalence. Last full suite64pass/1failure reused one Settings unique owner with a changed epoch; the public client correctly refused it. A fresh unique restart owner repairs that fixture, and both changed row/full suites pass. All prior logs/status/hashes/lifetime audits are preserved.

## Remaining boundary and compatible help

This is only supported-hold source profiles. Balanced automatic base-profile policy requires a separate contract; lid actions, critical countdown, full idle consumers, final supervisor/package PowerDevil retirement, installed and physical power qualification remain. Supported idle scopes remain zero. No Release-profile or installed/physical acceptance is claimed.

Platform queue and native sleep peer threads were read after verification. Worker is available, with no new implementation path claim. Concrete help offer: repair exact independent-review findings in this same isolated worktree; otherwise provide source-only public Power1/Settings1 authority or balanced-base-policy contract analysis to the next manager-assigned lane. Private reproductions require manager resource routing.

## Changed paths

- `docs/wiki/adr/0330-gate-native-source-profile-holds.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/architecture/power-policy.md`
- `docs/wiki/architecture/power-service.md`
- `mkdocs.yml`
- `ops/team/messages/pf-power-policy-sol-20261001/20261001T214234Z-claim.md`
- `ops/team/messages/pf-power-policy-sol-20261001/20261001T215550Z-source-checkpoint.md`
- `ops/team/messages/pf-power-policy-sol-20261001/20261001T223640Z-runtime-findings.md`
- `ops/team/workers/pf-power-policy-sol-20261001.md`
- `src/services/power_service/CMakeLists.txt`
- `src/services/power_service/app/main.cpp`
- `src/services/power_service/include/qindaqt/services/power_service/adapters/native_profile_authority.h`
- `src/services/power_service/include/qindaqt/services/power_service/adapters/power_profiles_collaborator.h`
- `src/services/power_service/include/qindaqt/services/power_service/power_collaborators.h`
- `src/services/power_service/include/qindaqt/services/power_service/power_service_coordinator.h`
- `src/services/power_service/include/qindaqt/services/power_service/source_profile_policy.h`
- `src/services/power_service/src/adapters/native_profile_authority.cpp`
- `src/services/power_service/src/adapters/power_profiles_collaborator.cpp`
- `src/services/power_service/src/adapters/power_profiles_operations.cpp`
- `src/services/power_service/src/power_service_coordinator.cpp`
- `src/services/power_service/src/source_profile_policy.cpp`
- `tests/services/power_service/CMakeLists.txt`
- `tests/services/power_service/support/fake_ppd_service.cpp`
- `tests/services/power_service/support/fake_ppd_service.h`
- `tests/services/power_service/support/private_bus.h`
- `tests/services/power_service/support/profile_policy_bus.h`
- `tests/services/power_service/support/profile_settings_source.h`
- `tests/services/power_service/tst_power_profiles_adapter.cpp`
- `tests/services/power_service/tst_source_profile_runtime.cpp`
