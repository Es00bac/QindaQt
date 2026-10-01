# REJECT exact native source-profile candidate 0c326be58a61810e33913a2daac71c2921541a78

- Candidate: `0c326be58a61810e33913a2daac71c2921541a78`; base `4011a663`; all30 changed-file SHA256 values equal own exact-source qinda review tree before gates. Product source/test/docs byte-identical to `faa71a16`; own board-only descendants do not change product.
- Verdict: **REJECT, P0=0/P1=1**. No source integration. Return one precise blocking repair to the same implementer, then this reviewer rechecks the exact repaired descendant.
- Reviewer: pf-power-review-sol-20261001, a different worker; owned only own board/replies. No product source edit, no shared integration edit, no laptop build/GPU/native work, no speech.
- Time: 2026-10-01T22:59:14Z. Compiler and private resources both released; every scratch process/root directly absent.

## Blocking P1: equivalent hold ordering retries a known refusal

`src/services/power_service/src/source_profile_policy.cpp:163-168` builds the refusal/admission key by concatenating profiles and hold IDs in publication order; specifically line165 concatenates hold handles without canonicalizing them. The line183 refusal guard compares that order-sensitive string to `m_failedKey`. Reversing the same external hold inventory therefore appears to be a new admission input and dispatches another `HoldProfile`, despite unchanged authority/source/preference/capacity and the known refusal. Repeated reorder publications can repeatedly retry a provider refusal. This violates ADR0330/power-policy's requirement that equivalent facts never retry and known refusals require material admission/preference changes or explicit retry.

One manager-authorized ignored scratch probe preserves the actual resident service assembly and all mandatory startup assertions. It copies the exact runtime fixture, adds two external holds, keeps the existing same-fact no-spin control, then reverses only those same two holds. No policy/adapter/service/header/binary was changed. Control remains1 hold request; the reordered inventory alone produces **Actual2 / Expected1** at copied fixture line248. The fixture's source change, complete compiler/link argv and hashes are retained separately from original required gates.

Executable reproduction in own qinda worktree:

```sh
python3 build/review-evidence/gate.py scratch-build python3 build/review-evidence/scratch/compile.py
python3 build/review-evidence/gate.py tests-scratch build/review-evidence/scratch/repro knownRejectionDoesNotSpin
```

- Scratch compile/link: exit0,8.005s, PID/PGID388844, core0, minMemAvailable17,996,222,464 bytes, only one copied translation unit; strict compiler flags borrowed from own exact-source compile_commands and linked unchanged fixture objects/libraries.
- Scratch replay: exit1;2Qt passed/1failed/0skipped,462ms. Actual Power1 subprocessPID389069 and private daemonPID389068, same explicit activation-free bus config SHA256 `9aa7bf317b1fc05b7ef605a47a7e300a0ffaab0bf7cb48c871c8f7c4a9f3a29b`; runner389064. All four compiler/runner/daemon/servicePIDs and `/tmp/repro-qPvdga` directly absent afterward.
- Scratch binary SHA256 `3b8c558df05ff7f69758299269cfc7cd7a303695d24d2c4e6b494d9b90882c8b`; retained source delta SHA256 `1e5d7cc6b38928224f3b1557c269bfe5ca959eee700400407dd1a838db99bf68`.
- Requested repair: make equivalent admission inventories share the same refusal key and add the order-only regression to the owned real resident runtime fixture. Preserve changed actual admission retry, known refusal no-spin, timeout no-replay, external holds and all13 original runtime behaviors. Same implementer owns repair; reviewer does not rewrite guards.

## Independent unchanged gates and provenance

Own qinda `build/review-evidence/` retains configure/build/static/docs logs, exact commands/status,30-file source manifest, compile_commands, five artifact inventories, process/temporary-root audits, dead-bus environment, copied source/delta and scratch result. Own build root is isolated; no mutable build-root copy. Private test HOME/XDG roots are temporary, ambient session/system bus paths dead, DISPLAY/Wayland/starter variables absent, core0. No host bus/profile/settings/sleep/display/PAM/hardware mutation.

- Strict Debug/sharedON/testingON/pluginOFF/strictON/host-uinputOFF own CMake configure: exit0/46.013s, actual compiler GCC15.3.0. Four exact targets plus declared actual resident dependencies: build125/125 exit0/38.014s, bounded `-j8 -l24`, core0, minMemAvailable16,522,940,416 bytes; global MAKEOPTS unchanged. CompilerPID385980 absent after completion.
- Unchanged `ctest --test-dir build/dev -V -R '^qindaqt[.]power-(source-profile-runtime|service-profiles-adapter|service-publication|service-operations|service-boundary)$' --output-on-failure -j1`: exit0;5/5CTests;65Qt/0fail/0skip (publication17,operations17,adapter16,runtime15 covering13 behavior rows),13.94s. Mandatory authentic service startup passed. All40 logged privatePIDs+runner387430 and27roots directly absent. Original five hashes equal before/after required and scratch gates.
- Own `tools/validate-docs`: exit0,485 Markdown documents/navigation. Own `mkdocs build --strict --site-dir build/review-evidence/site`: exit0. Direct executable/provider provenance: `/usr/bin/mkdocs`, version1.6.1 in Python3.14 system site-packages, `/var/db/pkg/dev-python/mkdocs-1.6.1/CONTENTS` tracks entrypoint. Software installed by root through Portage.
- Own source shape: exit0,44files, largest415 nonblank. Own static power boundary: exit0. Source/product diff against exact candidate and `git diff --check 4011a663 0c326be5`: exit0. Unrelated pre-existing test-shape warning remains outside this causal review; no new broad test gates claimed.

Five independent original artifact SHA256 values (unchanged across both executions):

| Artifact | SHA256 |
| --- | --- |
| actualPower1 | `cf382856981e9324be7f06e1929998e0927826f2e05e32fc9990aca343c3f9ae` |
| runtime test | `78976c347c9eef1a984a027819262c2d508f0e1e0474314970a43bf0a1b15d59` |
| publication test | `677207e3069a188cdf6142a522d0f9a1400a302a2cdbf555309c2c96fd9c68fe` |
| operations test | `24cd5cb11fd74c10348f36cb29b4e2eb40501cc16db93f2987e6df9339dee21e` |
| adapter test | `d3d16d349ee7776ccf3352f1b340a77f5777fc41708f9f75f74b7c6ad81d0c4c` |

## Audit scope and retained failure history

All30 changed paths audited plus adjacent public Settings and coordinator contracts. DefaultOFF/native-exclusive canonical subscribe-before-query PowerDevil gate, current-owner cancellation validation, borrowed destruction lifetimes, nonce own-hold cookie admission, balanced deferral/no ActiveProfile policy writes, serialization/quarantine/uncertain-release fences, publicSettings boundary and zeroidle scopes align apart from the demonstrated retry-key defect.

Primary [PPD API](https://upower.pages.freedesktop.org/power-profiles-daemon/gdbus-org.freedesktop.UPower.PowerProfiles.html) independently confirms supported hold types, targeted manual cancellation and caller-lifetime retirement. Retained implementer first all13 startup failures, equivalent-source test expectation failure and64pass/1same-ownerSettings failure were inspected with exact repaired mandatory-startup rows. No acceptance inferred from stale board prose or exit alone.

Balanced base policy, lid actions, critical countdown, complete idle consumers, supervisor/package PowerDevil retirement, installed and physical qualification remain deferred. This review makes no widerPF2 or installed retirement claim.

## Next action and compatible help

Return this causal P1 to the same implementer; preserve rejected exact candidate and this reproduction. Assign this reviewer to the exact repaired descendant, run original reproduction plus affected runtime/adapter/static gates; do not restart unchanged unrelated audit. Platform queue and relevant sleep/runtime peer evidence were read. Reviewer is available with no resource/path claim beyond own record/replies. Concrete help offer: recheck the repaired refusal-key equality and its actual private resident regression as soon the manager routes the exact commit and resources.

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
