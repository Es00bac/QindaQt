# Conditional Power retirement source candidate

- Exact base: 5b7f5b5910e262a4bf2be2326bc92fc97af4e2ac.
- Worktree/branch: everyday-power-startup-20261008 / worker/everyday-power-startup-20261008.
- Decision: manager retire-only narrow extension; no unconditional Power restart.
- Source scope: replaced_activation_owner.{h,cpp}, one installed Power executable macro in session_supervisor/CMakeLists.txt, owning focused tests/CMakeLists.txt and new tst_replaced_power_owner.cpp, existing modeled proc fixture stat fidelity.
- Docs: compositor-session and power-service owning sections; ADR0094 additive consequence; new reserved ADR0360 Proposed and minimal nav row.
- resident_service_refresh fixed list and main startup order UNCHANGED. No Audio/Clipboard/Bluetooth service/graph/UI edits, no settings/profile/brightness writes or new manager calls.

## Predicate and lifetime boundary

Power1 joins the closed reviewed replaced-activation names, not unconditional resident refresh. Current bus unique owner is resolved first; daemon PID queries address that unique name. Process identity includes same user, proc PID/starttime and raw exe. Production observes identity across pidfd_open, holds the kernel lifetime, then re-resolves current unique owner/PID and identical user/starttime/executable immediately before pidfd signal. Failed/missing observations, missing pidfd support or observed identity change grants no signal. There is no bare kill fallback. Power requires exact compiled installed executable path plus deleted marker; an unrelated deleted program is not enough. Healthy, absent and Private owners remain untouched, even with an injected proc root.

Existing Settings/portal retirement keeps its inclusion semantics with the same new lifetime hardening. Private witness factory is constructor-equivalent owning same-thread test seam, not a public presentation/service API. Tests inject signals only for their own private broker records; actual pidfd coverage uses explicitly owned child processes and copied proc stat/raw deleted marker modeling, never arbitrary host processes.

After signal, bounded wait observes name retirement; it does not assert replacement readiness. Failures return best effort; existing main ignores this receipt and proceeds to desktop. Ordinary Power clients, not this helper, activate packaged installed Power after retirement. Shell starts before Session1 registration, so no post-Session1-only activation claim. ADR0025 history preserved; no invented arbiter/winner takeover permission.

## Prepared acceptance and current results

Focused qindaqt.session-replaced-power-owner fixtures: matching deleted Power, healthy/wrong executable, other UID, unreadable/changed lifetime, changed unique owner and different PID, missing owner/bus, Private guard, signal failure, unresolved bounded return, actual held owned child healthy/replaced. Existing session-resident-service-refresh fixtures remain the regression boundary for shared Settings/portal retirement, resident list, manager routes and no private shared-manager action.

Direct permitted no-compiler gates: tools/validate-docs531 exit0; mkdocs build --strict --site-dir .cache/power-startup-docs exit0; git diff --check0. Shape: replacement helper about210nonblank, existing focused fixture360nonblank, new Power fixture192nonblank; no500-line decomposition threshold reached. These are source/static results, not native acceptance. No tests compiled/executed or runtime action by worker yet.

Request exact source independent review, then explicit focused compiler/private-broker lease. Compile targets qindaqt_replaced_power_owner_tests and qindaqt_resident_service_refresh_tests (publisher dependency), configured native MAKEOPTS unchanged. Run owning two CTest rows with isolated private bus created by test wrapper; no live host Power service/radio/action. Preserve failures and repair only owning source. Root currently owns integrated native resource lease; no overlap.

Immutable desktop r16/source2188, queued toolkit/Windows/Android and previous Claude WIP untouched. R17 source freeze still awaits accepted integrated qualification; this is not a package/install candidate.
