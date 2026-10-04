# Read-only unlock refusal and observer restoration findings

- Time: 2026-10-04T19:27:50+00:00
- Identity: collaboration agent `/root/native_permission_repair`
- Bound: source/version/process-start/executable-hash and fixed journal categories only. No live Unlock/Prompt/AttachSession call, observer refresh, credentials/encrypted store values, process memory/environment, configuration/source change, app/service/desktop restart or speculative patch.
- Observed incident inputs below are manager observations unless separately marked worker-verified.

## What the helper result means

Root ran immutable helper4b9b default plan then one execution with reviewed selection834934ce: result refused/unlocked0/cancelled0,exit1. User says they entered the laptop login password. Three exact imported matches still share one collection labelled Login, locked. Root's fresh provider policy is SettingsAvailable=true,LockOnScreenLock=true,ScreenLockAvailable=false,ScreenLocked=true,IdleAvailable=false,LockAfterIdleMinutes=10. Root separately reports actual compositor NativeLock1 Locked=false/Protected=false.

In final helper `.cache/mail-single-unlock.py:184`, refused is produced after a normal Unlock/Prompt completion and final exact searches when dismissed=false but fewer than three matching items remain unlocked. It is not the helper's timeout/transport/shape/sender/owner failure branch. Native `prompts.cpp:116` sends dismissed=true on wrong password; missing/denied/expired helper and user cancellation likewise finishPrompt(true) (`:82`, daemon60second timeout `:14`; helper30second timeout). Those produce cancelled or failed, not this ordinary nondismissed refusal. A / immediate Unlock reply could also reach final search, but the user observed the ordinary password prompt.

Concrete source path explains the exact result: repository_.unlock succeeds; `prompts.cpp:117` calls notifyCollectionState; `secret_service.cpp:132` calls policy.enforce BEFORE reporting collection state. `lock_policy.cpp:29` relocks if lock-on-screen is enabled and native screen state is not Unlocked, or idle minutes>0 and idle observer is unavailable/idle. Both confirmed current settings currently force relock. The prompt then appends completion and sends dismissed=false without rechecking persistent unlocked state. Inference from root's observed result plus these exact source predicates: this attempt is consistent with successful collection-password authentication followed by immediate policy relock; it should not be described as evidence the user typed the wrong password. Collection label/matching indices alone are not decrypted credential-value proof.

## Source and executable provenance

Exact old desktop source is `2b5db406bbbbfbd1d3feb4573f64c9af1e3b298d`, installedr13 `ab7fc8f3cc5cc1a5d6997e61a7d09cca475f967b`. Product diff for `src/services/keyring`, `src/platform`, `src/session_supervisor` is EMPTY, git diff exit0. Full change is nine metadata/docs/ops paths, primarily fork metadata selecting permission-list repair. There is no keyring daemon/prompt/attachment/observer API delta here.

Worker independently read permitted executable metadata:

| PID/component | running image | running image SHA256 | installed comparison |
|---|---|---|---|
|1188694 keyring|/usr/bin/qindaqt-keyring|7943d8d7c94f7c4fe399dd4c84cd6ca24ba55b673047fbd5ad23508ec10de758|matches current installed root:root0755|
|972246 supervisor|/usr/bin/qindaqt-session (NO deleted suffix)|3e76f8a67ad1346cadf991c2cc76e9c3bcbfc76a5365cd7ff7e5e0d51c5ae20a|matches current installed root:root0755|
|972162 compositor|/usr/bin/qindaqt-kwin (deleted)|a4030aa777d2d34d10bf0095825ba8136c8d2dbda51737c94568d9c43b7466e7|old running image; root keeps desktop|
|1356578 Mail|/usr/bin/qindamail (deleted)|ab7543d3cec07ebda0e7c7f45f63a07a1442533668771c9e8f234b43e19d4d6d|installed fixed Mail23a8136a7bbc146cba50b2f872fcb3397b52e80168b51fc7e7e660121c081f82|

Installed prompt image is root:root0755 SHAe8e03aa476149364f30f8c7aa83ddae7d46ba901cc8299d23f805a85d96acb15. No prompt process was inspected during user input. Root reports daemon start12:06:19/systemdMainPIDmatch/NRestarts2. Worker start ticks confirm daemon is later than compositor/supervisor (10260981 versus9250428/9250587 at100Hz).

Old deployed forkdd74 already exports RequestStateWithReceipt/stateReceipt (`qindaqt/session-lock/nativelockdbusinterface.*`); accepted24d changes permission decoding, not that observer producer API. Old/new `KWinDisplay` restricted interface blacklist does not include ext_idle_notifier_v1 or layer-shell (`src/wayland_server.cpp:140`). Thus this permission-list delta alone does not explain absent idle observer. This does not claim the physical observer path passed.

## Why both observers can be absent

The production daemon main constructs SessionDisplayBinding and ResidentLockPolicy but DOES NOT attach from ambient WAYLAND_DISPLAY (`src/services/keyring/app/main.cpp:56`). Plain systemd service ExecStart invokes qindaqt-keyring with no attachment operation. Ambient display can support prior-compatible native prompt fallback through openPromptConnection=-2 while explicit attachment.basename remains empty (`session_display_binding.cpp:20`). Consequently a visible native prompt is not proof screen/idle observers are bound.

`NativeScreenObservation` starts the lock monitor only for nonempty display.basename, and restarts on display.attached (`resident_lock_policy.cpp:14`). Idle observer's opener and admission both require a live explicit display, and it refreshes on attached (`:36,44`). Changing systemd activation env alone cannot establish those bindings.

Supervisor `KeyringSessionLifetime` attaches only its startup window: timer100ms, at most30 attempts, then stopped on success (`src/session_supervisor/src/keyring_session_lifetime.cpp:10,24`). It retains a dedicated unique bus connection but has no service-owner watcher/re-attachment for a later systemd daemon replacement. Later daemonPID1188694 versus original supervisor972246 and two observed service restarts is compatible with this missing replacement attachment. Source/read-only policy does not independently expose whether a prior session owner is still retained or a binding was revoked; do not claim exact private state.

## Supported non-desktop-restart recovery boundary

Native supported `org.qindaqt.Keyring1.AttachSessionWithDisplay(s nativeBasename)->b` at `/org/freedesktop/secrets` (`properties.cpp:89`) can bind the CURRENT daemon without restarting it IF no different sessionOwner is already retained. Existing different owner is refused; do not steal it. Accepted same-UID caller is selected as session owner, and CompositorAttachment enforces explicit canonical native basename,0700 actual-user runtime, socket ownership/no writable outsiders, SO_PEERCRED equality to current compositor bus PID/UID, SO_PEERPIDFD liveness, same native compositor/session owner lineage. It does not attest an executable/supervisor image (`compositor_attachment.h:20`). Preserve these checks.

Acceptance emits attached, starting both actual observers through their existing collaborators. Ordinary metadata policy must then show ScreenLockAvailable=true/ScreenLocked=false and IdleAvailable=true before another normal user unlock. Lock preferences10minutes/lockOnScreenLock remain unchanged. If either observer remains unavailable, do not announce recovery or retry password blindly.

**A one-off busctl/gdbus AttachSession call is unsuitable:** daemon stores the CALLER's unique bus name; `secret_service.cpp:162` quits when that owner disconnects. A successful ephemeral attach followed by client exit would retire the daemon, not establish durable native session ownership. Supervisor public Session1 exports only CanLogout/Logout (`session_service.cpp:43`); there is no exported keyring re-attach method to reuse its retained connection. Normal native source/API provides no read-only query returning private sessionOwner.

Smallest supported prospective runtime repair while preserving this desktop is therefore a separately manager-authorized RETAINED native session-lifetime client that calls the existing method once with the independently approved actual display, holds its connection while the actual session lives, pins/watches current compositor/provider/supervisor metadata, and exits only with explicit lifecycle consequences. It must not unlock/read secrets, weaken policy, supply fixture identity, or fabricate supervisor attestation. If current owner prevents attach, stop/report; do not restart daemon or bypass checks. No such client was created or run in this read-only task. A production automatic owner-replacement attachment repair requires a separately assigned reviewed source outcome.

## Journal evidence and next bounded gate

Worker `journalctl --user _PID=<keyring/compositor> --since12:00 -o json` returned0 with0entries each. Unit-only journal since09:00 returned0/14entries; fixed categories: unit start6,process exits2,restart scheduled2,unit failed2,start-limit0,prompt/admission error0,authentication error0. No raw journal message/record was exposed. Starts category includes Starting/Started, not six distinct daemon lifetimes. This confirms restart activity only, not its cause. Source intentionally discards daemon Qt messages (`main.cpp:32`) and helper stderr is /dev/null (`process_prompt_provider.cpp:72`), so log silence does not classify wrong password or admission failures.

Next bounded read-only gate: verify currently approved canonical native display socket and root-provided active compositor owner/PID metadata, plus current native Lock1 supports RequestStateWithReceipt; root's GetPolicyState flags already establish missing observers. Then manager may separately authorize a retained normal AttachSessionWithDisplay client and observe ONLY boolean policy recovery while keeping it alive. The current task performs no attachment or native connection probing. Do not use one-off attachment or systemd-env changes as observer proof; do not retry unlock until policy observers confirm availability.

Verification: fetch0; exact old/new product diff0; allowed sudo readlink/sha256sum succeeded; installed images/proc start metadata checked; fixed journal aggregate reads0; no source/config/credential/app/session state change by this worker. Immutable unlock helper remains4b9b unchanged. Own board/replies/cache only changed. Available for bounded retained-client preparation if routed, or further exact metadata/source questions.
