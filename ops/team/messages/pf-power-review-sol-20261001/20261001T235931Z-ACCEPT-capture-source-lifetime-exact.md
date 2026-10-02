# ACCEPT exact capture source-lifetime repair

- Time: 2026-10-01T23:59:31Z
- Verdict: **ACCEPT**, P0=0/P1=0 for the bounded native-lock scene-view lifetime repair; no broader capture milestone or startup-resolution claim.
- Exact fork candidate: `68c4d74f903b7e8990dd5fd5d509ec8154eac1d1`, base `447eed969a8692754d29f62f46cd177181ccbdae`. Production `src` diff against `42604a8f420bf55efe1d582bb2a8b394b916d8d8` is empty.
- All changed paths reviewed: `src/plugins/screencast/screencaststream.cpp`, `qindaqt/capture-authority/README.md` (67 insertions/4 deletions). Exact diff-check exit0. No product edits by reviewer.
- Isolated fork worktree/branch: qinda `~/.cache/pf-capture-lifetime-review-sol-20261001`, `worker/pf-capture-lifetime-review-sol-20261001`; exact candidate preserved in qinda fork hub. Own evidence remains ignored on qinda under its `build/`.

## Source and causal evidence

`close()` lines468–491 sets `m_closed` before callbacks, stops the pending timer, disconnects both cursor subscriptions and source notifications, releases unique source ownership into QObject parenting, posts `deleteLater`, then emits closure. Actual manager `closed` observer sends Wayland closure and uses stream `deleteLater`; parent ownership provides fallback cleanup without double ownership. PipeWire uses `pw_loop` plus compositor-thread Qt `QSocketNotifier`, without an independent callback thread. No move-to-thread path was found for these source/stream objects.

Actual `OutputScreenCastSource::pause()` destroys cursor/scene views; `Item::scheduleSceneRepaintInternal` holds raw-view snapshots and can reach stream close through synchronous `OutputLayer::repaintScheduled`. Deferring physical destruction preserves those views until that synchronous traversal returns. The ready continuation checks closed after outward signal; source setter/render continuations recheck authority before further source use; captureAllowed checks closed both before and after potentially reentrant authority evaluation. Queued/timer entry and active callback entry guards prevent released-source access. The final prequeue privacy check is preserved, and postqueue resize checks closed. Logical revocation therefore remains immediate while physical retirement waits for the event loop.

Adjacent unchanged authority contracts retain exact owner/pidfd checks, direct jobRevoked close, native screenAboutToLock closure, and job terminal removal before emitting revocation/retiring the helper. No owner-loss or capture policy relaxation occurs in this candidate.

Read exact consumer586c handoff and cause2e retained records, then actual scripts/status/logs. Original447 `native-admission-trace` Qt3/0/0 with runner1 remains a real negative: Qt totals alone never establish compositor survival. Retained diagnostic fault context identifies a non-executable anonymous RIP and first return mapped to exact447 ELF0x959c4c, virtual call at item.cpp508; other raw stack slots remain unvalidated candidates. Diagnostic runner's private dependency survival assertion failed. Repaired implementer unmonitored response2/Qt2pass1fail before lock and monitored node25/Qt3pass runner0 remain retained and distinct; monitor timing success alone does not resolve initial admission.

## Own exact build and actual replay

| Gate / command on qinda | Direct result |
| --- | --- |
| `python3 ~/.cache/pf-capture-lifetime-review-sol-20261001/build/build_exact_source_lifetime.py` | One exact candidate TU compile exit0/5.410s, plugin link exit0/.603s; core0; minimum MemAvailable14,089,964kB; bounded owned PGID/deadline/memory monitor; no global MAKEOPTS edit. |
| `python3 .../build/verify_artifacts.py` before/after replay | Exit0; two changed paths exact, source equal426; 33 unchanged link inputs and four output/source hashes stable; compiler PGIDs444356/444387 absent. |
| `python3 .../build/run_review_unmonitored.py` (stdout/stderr retained in `build/review-unmonitored-summary.log`) | Preflight0; runtime runner0/3.998s; Qt3pass/0fail/0skip in2708ms; first and only run, no monitor or retry. |
| `python3 .../build/verify_replay.py` | Exit0; all14 runner/descendant PIDs absent, all34 runtime manifest hashes stable, own plugin loaded in compositor log, private root absent, no cleanup errors/cores, no monitor/second-run output. |

Build used candidateCPP byte-identical copy with immutable447 include/MOC identity and unchanged original447 other plugin objects/framework. It did not edit or replace owner artifacts. Link argv/input hashes/output manifests/status are retained in `build/independent-source-lifetime/`. This is a focused plugin rebuild, not a complete new fork build or installation.

Unchanged ca299 consumer caller, original447 native driver, c03 dependency programs, original real broker, consent/input/producer and original runner assertions executed in a private namespace with temp0700HOME/XDG, dead ambient host buses and core0. Direct preflight shows PID2/uid1000/no capabilities, only AMD1002:731f renderD128, RO sysfs, no DRM primary card/input/sound/live host buses; renderer audit reports Radeon RX5700XT, not software. Actual source producer painted; PipeWire node25/serial25 decoded more than3 bounded real pixel frames. The unchanged lock row then requires live stream and pending helper death, retained file removal, pending denial response2, node removal, stopped decoded frame count, and post-lock response2/no helper. Runner0 additionally requires compositor/dependencies alive before cleanup. Own compositor log proves own rebuilt screencast plugin loaded. Actual group is `qindaqt-native-capture-zqrxnufc`; raw audits/logs/status/command/provenance under `build/review-unmonitored/`.

SHA256 identities:

- Own plugin: `ad45a7d18b98a370ca473eb6193ab2a27edaf8b0c526bc164b406a3437c38bb2`.
- Exact candidate CPP and compiled copy: `bc0382b01c941cc358165314d26899c2f9db413b3d03682c20647e286a4ef2c0`.
- Unchanged original447 driver: `5f06d13d4749cc1ace859080ef8ce1b6b28f0a4f65750b82331470dbbc2a1086`.
- Unchanged ca299 caller: `5d31d9193ea202f74fec8091edb130278aaa4d15b22ee439d81f6e3f7a8149ea`.
- Unchanged runner: `50d35a3fa3abda856ee528bcd1aca2bcac651334ebef4c6ab78054b0fa7f33dc`.

## Caveats, next gate and stopping point

One own unmonitored pass establishes this bounded lifetime/privacy regression with direct evidence; it does not establish resolution of recurring initial admission refusals. Full seven-case/three-compositor native matrix, compositor-loss coverage, deterministic startup receipt/ordering qualification, manager rebuild/rerun on integration, and Portage delivery remain separate gates. Production authorization OFF means native locked retirement coverage, not PAM/trusted greeter/unlock or recall of already consumed buffers. No live desktop/system install/routing changed. Reviewed README truthfully retains those limits and negative history.

Compiler released after its two PGIDs ended; private runtime released after all14 PIDs/root cleanup verified. Resources now **none**, worker available. Requested next action: manager may integrate exact68c as the bounded scene-view lifetime repair, perform affected integration rebuild/replay, and retain the startup/fullmatrix held state. Queue and peer threads read; concrete next compatible help offered: independently review the eventual bounded startup receipt diagnosis/repair candidate and its unchanged-admission causal evidence, without implementing it. Stop this review here; preserve all worktrees/evidence and prior power/package verdicts.
