# Audio1 exact repaired-candidate review: REJECT

- Reviewer: Codex Audio1 exact reviewer (different worker from implementer)
- Exact candidate reviewed: `e6423be9040edb5f28dc2f3d8d38665b7ad06030`
- Exact tree: `6516fe2094db0198606fd299836ac26e0b443dbe`
- Parent: rejected `6926aad9c93a757d06f32835db9962007ce2b195`
- Decision: **REJECT exact commit `e6423be9040edb5f28dc2f3d8d38665b7ad06030`**
- Findings: **P1 0 / P2 1 / P3 0**

The five prior P2 mechanisms were materially repaired, but one new blocking lifecycle race remains. Passing normal registries, sanitizer coverage, packaging, and activation do not override that exact-tree defect.

## Blocking finding

**P2 — a stopped disconnect-reset idle can poison later PipeWire recovery.** `wireplumber_worker.cpp:406-423` sets `m_resetScheduled` and defers reset to an idle; only `handleDisconnected()` at lines 425-433 clears it. The higher-priority explicit stop at lines 100-128 can clean/quit before that idle, while reused-run setup at lines 152-197 leaves the latch set. The next run's real loss then returns at line 410 and never publishes or reconnects. A private-runtime ordering probe reproduced `first_generation=1 second_generation=3 reset_still_scheduled=1 second_loss_observed=0` in the initial run and 2/2 repeats. Full record: `1787862747-codex-audio1-reset-latch-restart-finding.md`.

## Passing evidence on this rejected hash

- Exact identity/shape: detached exact HEAD/tree above, tracked tree clean; parent-to-HEAD whitespace clean. Docs validation passed 43 pages; source-shape passed 747 files with zero allowlist skips and changed `audio_client.cpp` at 476 nonblank; strict MkDocs passed.
- Fresh Debug: strict build `603/603`; exact Audio selection 6, `6/6`; full selection 89, `89/89` in 12.16 s.
- Fresh Release: strict build `603/603`; exact Audio selection 6, `6/6`; full selection 89, `89/89` in 10.98 s.
- Debug and Release lifecycle repetition: activation and production WirePlumber runtime each passed 10 consecutive repetitions, 20 executions/configuration. These normal schedules do not force the P2 ordering.
- Fresh ASan+UBSan: exact Audio selection 6, `6/6` in 3.66 s. A separate live disposable-PipeWire sanitizer probe forced 250 immediate connected start/stop cycles: descriptors 5→6 (+1), zero post-stop snapshots, no sanitizer report. Thus the prior rapid-stop leak/callback P2 is closed.
- Production/stage: Release shell build `403/403`; `all_qmllint` `2/2` with unchanged pre-existing warnings; install succeeded. Installed 11 Audio headers, three Audio archives, exact service executable/descriptor/XML/systemd unit; `ldd` had no missing dependency and `systemd-analyze verify` passed.
- Installed private-D-Bus lifecycle: 10 repeats, 20 exact staged activations. Each constructing bus loss removed the exact PID; replacement used distinct PID/owner/epoch; second loss left no exact process. Ten pass/no-orphan markers.
- Dependency direction remained protocol→client/service, with WirePlumber/GLib and private headers confined to service implementation. Public ownership/threading/error contracts and ADR/wiki changes otherwise matched the five repairs.

## Boundaries and cleanup

No host session bus/audio graph, physical audio, live desktop, input, compositor, or user service was touched. Physical USB/HDMI/Bluetooth/jack/microphone/speaker, multichannel, suspend/hotplug, realtime and integrated UI remain unqualified. All exact staged/reviewer service and private PipeWire children were terminated/reaped; no Audio reviewer `/tmp` runtime roots remained. Product tree remained clean and exact.

## Requested manager action

Do not integrate `e6423be9040edb5f28dc2f3d8d38665b7ad06030`. Review only a new exact commit that makes reset scheduling run-scoped/cancellable and deterministically proves two full loss→stop→restart→loss cycles, then reruns all prior lifecycle/resource/sanitizer/install gates.

