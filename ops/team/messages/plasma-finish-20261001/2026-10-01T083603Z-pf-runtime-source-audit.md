# Exact native-runtime source audit

Worker: pf-runtime-sol-20261001. Audited consumer base ab7640944d67b49f5fcbd198a96ac6c70cad193c; authoritative fork hub main 2e4e1961169febaf3e2a1f6d6eeede9510d18ac2 (read directly through ssh qinda). No production source edit or native session/build was made during audit.

## Integrated versus candidate source

`git diff --quiet HEAD <branch> -- <module>` establishes identical bytes for NativeLockRuntime, PowerClient and PowerService source against native-lock-runtime, NativeLockService against native-lock-services, NightLight service and Display Settings against native-night-light, and Input Settings against native-shortcuts. Non-ancestor commit lists alone would falsely report those already-integrated source changes as missing.

| Candidate branch | Exact current tip | Actual remaining difference |
| --- | --- | --- |
| origin/feature/native-lock-services | d38bd59289a716132b3411482edd1252baf562d1 | Manual request/facade already integrated |
| origin/feature/native-lock-runtime | cc72c416fced3bab6cb9c34a29e936e0b9d38892 | Admitted-FD DPMS IdlePolicy module, runtime composition and per-source Power Settings absent |
| origin/feature/native-night-light | a81d3a8a44ea3c2d95d6083085ddf6bacaafdf5b | Consumer service/UI already integrated; fork manifest stale |
| origin/feature/native-power | 151dce49f4223ea5cc3d9b9ecae27371fd3b96da | PF1 schema/import integrated plus preservation repairs; branch is not a PF2–4 runtime implementation |
| origin/feature/native-shortcuts | 59aeb31e16c3fb2f7a2f946d0bea971e64a03c4a | Documentation/checkpoints only; Input Settings production code unchanged |

## Native lock and power

PF5–7 server/greeter/PAM source is integrated at the pinned fork, according to direct source inspection and exact manifest boundary. Existing handoff evidence is historical evidence, not rerun by this audit. Physical DRM/input, installed executable/PAM binding and real-owner credential acceptance remain release qualification.

PF8 source composes native Lock1/ScreenSaver facade, authenticated native state/request receipts, Settings1 lock provider and actual idle observer in qindaqt-session. Automatic idle/grace/manual/resume behavior and Power1 receipt consumption exist. Remaining production gaps are substantive:

- Shell notification presentation still constructs legacy QtSessionLockTransport/SessionLockStateMonitor in src/shell/runtime/shellruntimeapplication.cpp:514. The monitor requires compositor and both ScreenSaver names to share a unique owner/PID; native facade owns ScreenSaver in the supervisor. Therefore native session composition cannot satisfy its Unlocked quorum, suppressing notification disclosure. Migrate this consumer to the public authenticated native observer/attachment boundary without weakening Unknown behavior.
- Runtime::requestSuspend waits for actual Protected, but NativeLockComposition has no caller. SessionActionsClient::dispatchMutation calls logind Suspend directly. Owned lock-before-sleep ordering is not composed.
- Native Lock1 does not handle logind Lock/Unlock, LockedHint, or hold/release a PrepareForSleep delay inhibitor. The current Power logind adapter observes PrepareForSleep, which supports resume behavior only.
- ScreenSaver Inhibit/UnInhibit and duration/activity methods return Unsupported; all three idle scopes must have real consumers before compatibility can accept a lease.

PF1 bounded schema/import exists. Power1 contains owner-bound idle lease core/methods and targeted receipt transport. Its registry defaults to zero consumed scopes and PowerServiceObject never sets nonzero scopes. No host inhibition can truthfully be claimed. PF2–4 missing pieces include native lid block-inhibitor/action/docked policy, critical-battery countdown/action, per-source profile application, keyboard brightness ownership, dim and idle-suspend stages, scope capability composition, common PowerManagement/ScreenSaver/portal adapters and applet inhibition/override presentation. PowerDevil remains a supervisor child; desktop-controls owns PowerDevil brightness feedback and idle binding; Settings Power still uses PowerDevil lid/profile ports. These are implementation gaps, not merely packaging or hardware checks.

Exact absent DPMS tail commits, in order:

1. 7a8a1a452f4d332712f8430c6a9253eef34fc488 — Run display-off policy on the admitted compositor.
2. 7972e7ecc0a4caa2eb7cb2f9131dc319e8d2c239 — Repair admitted Wayland DPMS teardown.
3. 62e791544ea84d4a9668ed62301cc84bce812b59 — Expose admitted display power through IdlePolicy.
4. d469255ac2920e8a0c4e9a5e336e5065fac1b066 — Route display off through per-source native preferences.
5. cc72c416fced3bab6cb9c34a29e936e0b9d38892 — Document per-source native display-off behavior.

Candidate intentionally retains zero Power1 supported scopes, separate auto-lock timer, admitted ordinary FD only, and no native suspend/dim behavior. It has an in-process real Wayland DPMS lifecycle fixture, not an actual nested compositor proof. Selectively replay these five commits and preserve current independent work; never merge the stale whole branch.

## Night light and fork source pin

Manifest pins fork 6ab6c01ede8143a7ddb477d6f0040e9b2f3753e4, tree 99a81290288d4f72742fd85ae4fb21b752ecd224. Authoritative main is four commits ahead:

- c9561e5cdc1ab81a6ce64a19e751c41aa2c25749 — Move night light scheduling to QindaQt.
- 5f6fdf1124f4e600cc77129be91b09e0d2880b86 — Resolve night light opt-outs by output identity.
- 78eb301effec7c4ce895dd9ce83f54c3a1b8610a — Add native shortcut registry core.
- 2e4e1961169febaf3e2a1f6d6eeede9510d18ac2 — Model shortcut action activation state.

NightLight service/UI/GeoClue/import consumer source is already present and byte-identical to its candidate. The pinned producer still uses old schedule logic, so package/source-pin convergence and combined private producer/consumer qualification are missing. Moreover authoritative main root CMakeLists.txt:150–154 still discovers KNightTime with package TYPE REQUIRED and purpose Needed for Night Light, even though the native plugin no longer links it. Remove that remaining PF12 configure dependency and prove configuration without KNightTime, then pin and qualify exact coherent fork/consumer source. Per-output opt-out, unavailable/owner-loss neutral behavior, fixed/manual/consented automatic location and temporary screenshot/color inhibit need combined executable proof. Physical calibrated outputs remain separate qualification. PF10/11 Activities/QML removal is integrated in the current fork pin; PF14 packaging/profile-parent delivery belongs to manager/overlay scope.

## Native shortcuts

PF22 pure registry/store core and runtime active-state support landed on authoritative fork main but are not in the consumer pin. Default GlobalShortcutsManager still embeds KGlobalAccelD. Fork candidate d7e236e010ed53cf187621803d3b06b905024cd7 adds experimental compatibility endpoint/input wiring behind KWIN_EXPERIMENTAL_NATIVE_SHORTCUTS=OFF; this candidate branches from the older pin and cannot wholesale replace main (it would regress native night light).

The experimental endpoint has no actual bus-owner cleanup, Store load/save or concrete legacy importer composition. It retains KGlobalAccelD build dependency/linkage. The registry's in-memory owner field holds a friendly component label rather than an authenticated unique bus owner. The previous worker explicitly removed stalled/fatal-warning owner-cleanup experiments; its claimed private/nested test results do not qualify owner lifecycle. PF23 still needs persistence/default precedence, importer, caller ownership, modifier-only/pointer cancellation/layout behavior and complete lock/inhibitor/shell lifecycle evidence before default activation and dependency retirement. PF24 Shortcuts1 does not exist; Settings uses KGlobalAccel compatibility and custom desktop files, supervisor still starts kglobalacceld, portal switch-over remains separate.

## Executed evidence and next work

Read-only git/source audit and ssh hub inspection exited 0. Automated source equality check printed seven identical module comparisons and passed three assertions (IdlePolicy absent, no composed Power1 supported scopes, no native suspend caller). `./tools/validate-docs` exited 0: 475 Markdown documents/navigation. `git diff --check` exited 0. No CTest/build/native acceptance is claimed.

Manager has now explicitly assigned selective DPMS/per-source tail recovery as the next bounded repair. Required gates: affected module build, real private DPMS lifecycle fixture, pure stage/source Settings tests, migration/import preservation, adjacent native Lock1 and Power1 receipt/registry gates, strict MkDocs/link validation; manager controls any actual nested resource slot and subsequent independent exact-commit review. PF2–4 completion and unrelated fork/nightlight/shortcuts changes remain excluded. After candidate handoff, offer narrow review/help and wait for explicit next assignment.
