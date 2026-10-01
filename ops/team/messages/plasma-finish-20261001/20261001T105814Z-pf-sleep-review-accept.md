# ACCEPT exact native protected sleep candidate

- Candidate: e6072eb1d736fa017ae3d9de0aa5385da42dd34c
- Reviewer: pf-sleep-review-sol-20261001
- Time: 2026-10-01T10:58:14Z
- Base: 53bf486bac9c067e5fff8143e2457e69e4a7b01d
- Verdict: ACCEPT; no blocking source, contract, documentation or focused-gate findings
- Requested next action: Program Manager integrates this exact accepted candidate and reruns affected combined-tree/ABI gates; this review does not claim integration or installed qualification

## Independent review scope

Reviewed all candidate product paths: NativeSleep public boundaries, selected-session login1 adaptation, coordinator/facade, NativeLockRuntime lifecycle delta, SessionActions split/client route, supervisor composition, build/test registration, focused/adversarial tests and affected wiki/ADR/navigation. Only reviewer records were edited.

The session join checks selected ID plus actual supervisor PID through equal GetSession/GetSessionByPID paths, exact Id and User `(uo)` UID/canonical user path, and daemon-resolved root-owned unique logind owner. Production readonly admission retains the live supervisor/ordinary compositor socket/PIDFD boundary. Exact sender/path/signature signals, PreparingForSleep snapshot/signal supersession, selected SessionRemoved, owner replacement and generation retirement fence authority and late descriptors. One owned CLOEXEC duplicate closes on stop/revocation; Qt owns rejected wire descriptors.

Native Locked/Protected current nonce observation precedes both manual Suspend and delay release. Lock requests admission; Unlock only refreshes observation. Unknown/Locking and failed native admission do not admit manual sleep or release the retained delay. Protection and coordinator serial are rechecked after CanSuspend. Stop and authority loss retire callbacks, resume rearms a fresh inhibitor, confirmed lock-on-resume state remains gated, and stopped runtime Settings changes cannot rearm idle. Sleep1 resolves actual caller UID and is owned by Session1; Changed converges bounded async availability. SessionActions has no direct login1 Suspend or alternative locker fallback. Unconfirmed dispatched operations return Uncertain without replay.

## Direct acceptance evidence

Reused immutable qinda source binaries, not a reviewer compilation: observed qinda worktree HEAD `3e9fcaccad6b65fa5d872b36d8e6e0012284d4a6`; authoritative hub `git diff --quiet 3e9fcacc e6072eb1 -- src tests docs mkdocs.yml` exit 0. Existing focused binaries were independently executed after manager private-runtime grant; no compiler reservation taken.

```sh
ctest --test-dir /home/cabewse/work_SPaC3/container-wm.worktrees/pf-power-sleep-20261001/build/focused --output-on-failure -V -R 'qindaqt\.(logind_sleep_transport|sleep_coordinator|session-native-lock-runtime|session-actions-client|session-actions-boundary|session-actions-boundary-poison|qt_native_lock_request)$'
mkdocs build --strict
tools/validate-docs
git diff --check 53bf486bac9c067e5fff8143e2457e69e4a7b01d e6072eb1d736fa017ae3d9de0aa5385da42dd34c
tools/check-source-shape --root build/evidence/changed-source --config tools/source-shape.json --warnings-as-errors --largest 3
```

- Private CTest exit 0: **7/7** rows in **37.25s**, no skips. Qt totals: SessionActions10, Runtime12, Logind17, SleepCoordinator16, NativeRequest16 = **71 passed, 0 failed, 0 skipped**. Boundary and poison are two additional CMake rows.
- Local strict MkDocs exit 0 (6.72s); docs validator exit 0, **480** documents/navigation; exact candidate diff-check exit 0.
- Focused changed-source shape exit 0, **29** files, 0 allowlist skips/issues; largest production source400 nonblank lines.
- Full tracked-source shape executed on exact base and candidate materializations using unchanged policy: both exit1, **168** inherited issues, issue sets exactly equal, zero introduced/resolved. No unrelated cleanup requested.
- Reviewer logs preserved in ignored review `build/evidence/review-runtime.log`, `review-mkdocs.log`, `review-docs.log`, `review-shape.log`, and base/candidate JSON.

## Bounded caveats and resources

Tests use disposable brokers, ordinary socket/kernel PIDFD and production targeted nonce ports, and pipe-backed descriptors. No host system bus, host lock/sleep, PAM/keyring/credentials, installation or service changes. Existing Debug/plugin-OFF binaries establish process/wire behavior, not compositor ABI, physical DRM protection, installed logind timing, PAM or hardware resume. A logind delay remains finite; retaining the FD cannot veto a privileged external request past its system deadline. Source-specific idle/lid/button policy and Power1 scope advertisement remain separate.

Private runtime was released immediately after its gate; compiler was never claimed. Review branch and authored records are preserved to qinda. This ACCEPT applies solely to exact candidate e6072eb1.
