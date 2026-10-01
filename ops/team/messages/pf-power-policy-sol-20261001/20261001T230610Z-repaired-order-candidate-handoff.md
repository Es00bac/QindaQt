# Repaired native source-profile candidate: inventory order invariant

- Exact candidate: `ce84b0a5610c60d2111601bf08018f8c2ea8cea6`; base `4011a663`; pushed `worker/pf-power-policy-runtime-20261001`.
- Supersedes rejected candidate `0c326be58a61810e33913a2daac71c2921541a78` only for the independent P1 order-sensitive refusal key. Same reviewer pf-power-review-sol-20261001 must recheck this exact descendant before manager integration.
- Since rejection, product/documentation changes are only SourceProfilePolicy's canonical admission key, mandatory known-refusal runtime row, primary power-policy wiki and ADR0330 consequence. Later operational records change no source/test/docs/CMake bytes.
- Compiler and private runtime both RELEASED. No host profile/settings/power action, software installation, laptop build/GPU or shared cache edit. Board AVAILABLE; no new implementation paths claimed.

## Repair and evidence

The refusal key now sorts supported profile IDs and external hold opaque IDs independently. Equivalent provider-array order yields identical admission; actual membership, selected preference and authority/epoch changes remain material. The mandatory real resident knownRejectionDoesNotSpin row uses two external holds, retains the original repeated-fact control, readback-confirms reordered holds and supported profiles, checks no repeated rejection dispatch, then proves a changed confirmed preference admits a new request. No startup or prior uncertainty/manual/default/scopes assertion was removed.

Own qinda `build/pf-source-profile-evidence/` retains all previous failures plus this exact repair:

- `cmake --build build/dev --target qindaqt_source_profile_runtime_tests qindaqt_power_profiles_adapter_tests -- -j8 -l24`: exit0,13.820s,9 actual Ninja actions, compilerPID/PGID400006, initialMemAvailable18,066,416KiB; core0/min3GiB30s ownPGID-stop contract. Actual policy TU/archive, resident relink and test target rebuild recorded in `build-order-repair.log`/status. Existing strict Debug/sharedON/testingON/pluginOFF/host-uinputOFF configure and accepted fork690 prefix remain unchanged.
- `qindaqt_source_profile_runtime_tests knownRejectionDoesNotSpin`: exit0,3 Qt passed/0failed/0skipped,766ms (`tests-order-repair-single.log`/status).
- `ctest --test-dir build/dev -V -R '^qindaqt[.]power-(source-profile-runtime|service-profiles-adapter|service-publication|service-operations|service-boundary)$' --output-on-failure -j1`: exit0,5/5 CTests,65 Qt passed/0failed/0skipped,14.32s. Runtime15 Qt cases cover all13 behaviors; publication17/operations17/adapter16 unchanged (`tests-order-repair-full.log`/status).
- Five executable hashes plus PowerService archive stable before/after. Runtime executableSHA256 `6825570d9260d04a3e20baa31968b8ec339c02714e517fa385f175ab81ab2745`; actual residentSHA256 `23429560decef53c7816e43fe49cef3e0e210c09929c5a95143d20c14197898e`; archiveSHA256 `0c303fc26f410ff430e19d8ccbdd6aac9cbe0b755b2ca249a4dfb50ab388db6f`. `order-artifacts-before/after.json` retain all6.
- All42 logged private PIDs+2 runnerPIDs and28 logged temporary roots directly absent after the single+full gates (`order-lifetime-audit.json`). Untyped no-host-include/no-activation bus configSHA256 `9aa7bf317b1fc05b7ef605a47a7e300a0ffaab0bf7cb48c871c8f7c4a9f3a29b`; dead ambient session/system bus paths, temporary HOME/XDG, no DISPLAY/Wayland/starter env, core0 (`order-environment.json`).
- On **qinda**, `tools/validate-docs`: exit0,485 Markdown documents/navigation/links. Portage-provided `/usr/bin/mkdocs` observed version1.6.1/Python3.14; `mkdocs build --strict --site-dir build/pf-source-profile-evidence/site`: exit0/7.59s. The previous qinda tool-pending gate is now satisfied. `repair-static-status.json` records these and boundary, both source-shape and diff gates, all exit0. Existing unrelated518-line activation-test review warning remains.

Independent original REJECT/reproduction remains preserved under reviewer branch68259b8a/thread20261001T225914Z. Parent and same reviewer received exactce84 gate results; no acceptance/integration inferred from own passing checks.

## Remaining boundary and help

Default policy remains OFF until explicit native-exclusive authority. Balanced automatic base policy, PF2 lid/critical countdown, full idle consumers, final PowerDevil supervisor/package retirement and installed/physical power qualification remain separate. Idle scopes remain zero; external/manual holds and uncertain no-replay fences remain covered.

Platform queue and exact independent REJECT thread were read after gates. Requested next action: same reviewer exactce84 rereview, then manager integrated affected gates. Concrete available help: reproduce/repair an exact rereview finding in this worktree, or provide manager-routed source-only public Power1/Settings1 authority/balanced-contract analysis. No new paths/resources claimed.

## Changed paths from exact base

- `docs/wiki/adr/0330-gate-native-source-profile-holds.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/architecture/power-policy.md`
- `docs/wiki/architecture/power-service.md`
- `mkdocs.yml`
- `ops/team/messages/pf-power-policy-sol-20261001/20261001T214234Z-claim.md`
- `ops/team/messages/pf-power-policy-sol-20261001/20261001T215550Z-source-checkpoint.md`
- `ops/team/messages/pf-power-policy-sol-20261001/20261001T223640Z-runtime-findings.md`
- `ops/team/messages/pf-power-policy-sol-20261001/20261001T224459Z-runtime-candidate-handoff.md`
- `ops/team/messages/pf-power-policy-sol-20261001/20261001T224605Z-qinda-doc-tool-pending.md`
- `ops/team/messages/pf-power-policy-sol-20261001/20261001T230006Z-inventory-order-repair-claim.md`
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
