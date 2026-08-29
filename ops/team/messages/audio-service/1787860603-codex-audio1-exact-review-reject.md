# Audio1 exact-candidate review: REJECT

Reviewer: Codex Audio1 exact reviewer  
Candidate reviewed: `6926aad9c93a757d06f32835db9962007ce2b195`  
Base verified: `dc29c88911f0ed6d381211027f16f46bbf92a07c`  
Decision: **REJECT exact commit `6926aad9c93a757d06f32835db9962007ce2b195`**

The candidate is not integration-ready. Five reproducible P2 contract/lifetime
findings remain in the exact code. There are no P1 findings. Passing registry,
packaging, and installed lifecycle gates do not override these product defects.

## Blocking findings

1. **P2 — public completions violate the asynchronous API contract.**
   `audio_client.h:23-27` promises all completion/error reporting through
   asynchronous signals, but `audio_client.cpp:318-345` directly emits local
   Busy/Rejected/Unsupported completions before the public call returns its
   request ID; `tst_audio_client.cpp:132-154` encodes the synchronous behavior.
   Full record: `1787859086-codex-audio1-exact-review-finding.md`.

2. **P2 — stop is not a snapshot publication/generation barrier.**
   `wireplumber_audio_backend.cpp:20-37` queues untagged callbacks to Qt;
   `audio_operation_coordinator.cpp:79-87,236-269` neither rejects post-stop
   callbacks nor fences run generations/revision regressions. A production
   backend probe observed post-stop public snapshot publication in `100/100`
   iterations. Full record:
   `1787859294-codex-audio1-stopped-snapshot-finding.md`.

3. **P2 — client lineage accepts contradictory authority data.**
   `audio_client.cpp:270-295` accepts same-epoch/same-revision snapshots whose
   payload differs; `audio_client.cpp:433-455` can accept an old-epoch success
   after the current published snapshot advanced epoch under the same owner.
   Existing tests cover only strict revision regression and initiator mismatch.
   Full record: `1787859540-codex-audio1-client-lineage-finding.md`.

4. **P2 — backend outcomes can bypass the public wire validator.**
   `audio_backend.h:19-23` exposes unconstrained backend reason text;
   `audio_operation_coordinator.cpp:272-311` copies it verbatim while sanitizing
   only diagnostic, although `audio_validation.cpp:217-240` requires a bounded,
   NUL-free complete result. A malformed backend therefore makes the authority
   send a protocol-invalid D-Bus reply. Full record:
   `1787859861-codex-audio1-backend-result-validation-finding.md`.

5. **P2 — rapid production-backend start/stop deterministically exhausts FDs.**
   `wireplumber_worker.cpp:167-221` starts component/connect async work with raw
   worker callback data; `wireplumber_worker.cpp:86-114,398-419` drains only
   operation-sync callbacks before destroying the worker context. An ASan+UBSan
   private-runtime probe ran 50 immediate cycles and measured
   `FD_BEFORE=5 FD_AFTER=506 FD_DELTA=501`; 250 cycles aborted in GLib with
   `Creating pipes for GWakeup: Too many open files` (exit 133). Full record:
   `1787860168-codex-audio1-rapid-stop-resource-finding.md`.

## Independent command evidence

All builds/tests used fresh reviewer-owned directories in the detached exact
candidate worktree.

- Exactness and shape:
  - `git rev-parse HEAD` -> exact `6926aad...`; merge-base -> exact
    `dc29c889...`; `git diff --check dc29c889...HEAD` -> status 0.
  - `./tools/check-source-shape --largest 20` -> status 0, 746 files checked,
    zero allowlist skips; largest changed production file was
    `audio_client.cpp`, 436 nonblank lines.
- Debug:
  - configured strict Debug, shared libraries, tests enabled, shell/KWin/host
    uinput disabled; `cmake --build ... -j2` -> status 0, `602/602`.
  - `ctest -N -R '^qindaqt[.]audio-(protocol|client|qt-transport|activation|service|wireplumber-runtime)$'`
    selected exactly 6; matching CTest -> `6/6` pass.
  - `ctest -N` selected 89; full `ctest --no-tests=error --output-on-failure -j2`
    -> `89/89` pass.
- Release: identical registry options with `CMAKE_BUILD_TYPE=Release`;
  `602/602` build, exact Audio selection 6 and `6/6` pass, full selection 89 and
  `89/89` pass.
- Lifecycle repeat: in both Debug and Release,
  `ctest -R '^qindaqt[.]audio-(activation|wireplumber-runtime)$' --repeat until-fail:20`
  -> both tests passed all 20 runs (40 executions per configuration).
- Sanitizers: fresh Debug Audio targets with
  `-fsanitize=address,undefined -fno-omit-frame-pointer`, strict
  `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1` and strict
  UBSan -> selected 6, `6/6` pass. The separate rapid-stop regression probe
  intentionally returned 1 with the P2 FD evidence above.
- Production/QML: fresh Release, tests off, shell + production shell on, KWin
  off -> status 0, `402/402`; `all_qmllint` -> status 0, `2/2` targets. QML lint
  printed existing warnings in unchanged shell QML; Audio1 changes add no QML.
- Docs:
  - `./tools/validate-docs` -> status 0, 43 Markdown documents plus navigation;
  - `uvx --offline --from mkdocs==1.6.1 mkdocs build --strict` -> status 0;
  - source-shape and whitespace/diff checks -> status 0.
- Stage/install:
  - production configuration used an absolute reviewer prefix; build, QML lint,
    and `cmake --install` -> status 0.
  - installed 11 Audio public headers, three Audio static libraries, executable,
    exact D-Bus descriptor, XML, and systemd user unit; `ldd` had zero missing
    dependencies; `systemd-analyze verify` passed.
  - descriptor `Exec` and unit `ExecStart` both resolve to the exact staged
    executable; service/interface/name are `org.qindaqt.Audio1`.
- Actual installed private-D-Bus lifecycle:
  - used the installed descriptor unchanged, exact staged executable, private
    `dbus-daemon`, private empty XDG/PipeWire runtime, and no user session bus or
    host audio graph;
  - ten full repeats passed. Each repeat activated process/owner/epoch, killed
    the constructing daemon and observed prompt exact-process exit, then used a
    second daemon with reserved owner and proved distinct PID, owner, and epoch;
    second loss also left no exact process. This is 20 installed activations and
    20 daemon-loss exits.

Logs are retained under ignored reviewer paths
`build/audio1-review-{debug,release,sanitize,production-stage,installed-runtime}`
and `build/audio1-review-probes-sanitize`.

## Boundaries, caveats, and cleanup

- No live desktop, user session bus, host PipeWire/WirePlumber graph, physical
  audio, input injection, compositor, KGlobalAccel, or UI interaction was used.
- Not qualified here: USB/HDMI/Bluetooth, jack sensing, real microphone/speaker,
  multichannel semantics, suspend/resume, physical hotplug churn, realtime
  scheduling, CPU/PSS budgets, or future Audio settings/applet UI.
- Existing isolated null sink/source tests do cover graph observation,
  default/volume/mute/move operations, WirePlumber restart, and stale handle.
- Final cleanup found zero exact staged Audio1 processes and zero reviewer
  private-runtime fixture directories. Final product worktree is clean (ignored
  reviewer build evidence only), detached at exact `6926aad...`; base-to-HEAD
  whitespace check remains clean.

## Requested manager action

Do not integrate `6926aad9c93a757d06f32835db9962007ce2b195`. Have the
implementer repair all five P2 findings in the existing isolated implementation
worktree, post one new exact commit and evidence handoff, then assign that exact
repaired commit for independent re-review. The passing gates above apply only
to this rejected exact candidate and do not approve prose or an unreviewed
repair.
