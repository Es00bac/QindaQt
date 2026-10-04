# Native keyring owner-replacement reconnect handoff

- Worker: `/root/native_permission_repair`
- Time: 2026-10-04T20:11:15+00:00
- Status: available; compiler/private-runtime resources released
- Exact candidate: `f2f2d3927e235f585fe0ef0ab5ffec3f4ba52f42`
- Tree: `2a57242202a4e41a6877db20c325c8489c4149c6`
- Parent/exact assigned base: `eeed1f6cac509990f64fd1f692a796f370206c55`
- Qinda hub: `/home/cabewse/git/container-wm.git`
- Preserved branch: `fix/keyring-owner-reconnect-20261004`
- Clean implementer worktree: qinda `/home/cabewse/work_SPaC3/container-wm-keyring-reconnect-20261004`

## Outcome and contract

A persistent native owner watcher on the supervisor's existing dedicated bus renews approved display attachment after daemon replacement. Each actual fresh owner has a bounded30-attempt asynchronous admission batch, one in-flight call,500ms call deadline; metadata queries are250ms. The retained login display, no-auto-start requests, exact unique owner and same-UID checks preserve native first-owner/compositor admission. No policy or trust boundary is weakened and no production process dependency is added.

A replacement clears old admission and retires pending generations. Transient GetNameOwner/UID transport uncertainty differs from confirmed NameHasNoOwner or a real UID mismatch: unavailable metadata retains bounded retries but cannot accept attachment or Shutdown. An arrival hint can start a fresh window while unavailable; each request still requires actual current same-UID metadata. Stop/destruction retires watcher/replies before child waits and addresses only the still-current exact admitted owner; no well-known-name or unadmitted replacement Shutdown.

General supervisor test options now explicitly disable unrelated installed Night Light. Before this fixture repair, unchanged options/header/supervisor source selected `/usr/bin/qindaqt-night-light-service`, produced a fourth stop role versus expected3 and caused broad4/5 failure. Keyring is disabled in that failed case. The narrow14 synthetic-option changes leave production and dedicated Night Light role tests untouched; final broad5/5 passes.

## Changed paths

- `src/session_supervisor/src/keyring_session_lifetime.{h,cpp}`
- `tests/session_supervisor/CMakeLists.txt`
- `tests/session_supervisor/tst_session_process_supervisor.cpp`
- `tests/session_supervisor/keyring_lifetime/CMakeLists.txt`
- `tests/session_supervisor/keyring_lifetime/keyring_endpoint.cpp`
- `tests/session_supervisor/keyring_lifetime/keyring_owner.cpp`
- `tests/session_supervisor/keyring_lifetime/private-bus.conf`
- `tests/session_supervisor/keyring_lifetime/test_owner_replacement.py`
- `tests/session_supervisor/keyring_lifetime/tst_keyring_session_lifetime.cpp`
- `docs/wiki/architecture/{compositor-session,keyring-daemon}.md`
- `docs/wiki/adr/0348-reattach-keyring-after-owner-replacement.md`
- `docs/wiki/adr/index.md`
- `mkdocs.yml`

15 exact Git paths. Production lifetime implementation is146 nonblank lines. Only additive parent test registry/index/nav edits. Manager's D-067 Mail wiki paragraph is not in eeed base; our keyring wiki hunks replace the existing session-lifetime and verification paragraphs only. Preserve D-067 on merge. Manager checkpoints f6/6c advanced docs/ops, not assigned production source.

## Executed qinda-only gates

- Strict focused configure/build: exit0 with `-j24 -l24`; actual lifetime, optional child and supervised launcher sources, no replacement admission implementation.
- Full production configure/build: exit0, actual `qindaqt-session` and affected normal-registry targets. Initial default-prefix configure failed existing fixed fork capture BrokerExecutable contract; ordinary `CMAKE_INSTALL_PREFIX=/usr` and `KDE_INSTALL_LIBEXECDIR=libexec` resolves it without source workaround. Early fixture overload/name and strict shadow compile errors repaired before final passes.
- Final normal affected CTest: exit0, **5/5**, **37 Qt checks** (supervisor21+portal5+lifetime11), **2 Python cases**, no failures/skips,84.01s. Includes stable owner, retained caller/display replacement,30-refusal/fresh owner, stop race, delayed old success, no late callbacks, late startup arrival, legacy no-display and transient lookup recovery.
- Metadata timeout case pauses only a private registry whose same UID and `/proc` parent match this test's own `dbus-run-session` parent; a separate600ms thread resumes it. Actual250ms lookup expires, same-owner attachment recovers with no subsequent owner-change event. No live bus/process is eligible for the pause.
- Actual resident/software compositor **2/2**: empty private storage, compiled daemon and real Settings, standard idle/server receipt fixture. Replacement recovers ScreenLockAvailable/IdleAvailable, unlocked native screen observation and enabled preferences;60000ms protocol timeout and lock/unlock event observations work. Non-native display remains unavailable/ScreenLocked and unadmitted daemon survives stop. **No Prompt/Unlock/OpenSession/GetSecret, record, password, credential or physical-device calls.** Owned private children reaped4+5, survivors0.
- Exact unchanged-eeed replacement negative control: exit**1 expected**,2pass/1fail, second attachcount0 versus1,15.424s. Product source remains unchanged in qinda detached `/home/cabewse/.cache/container-wm-keyring-reconnect-negative-20261004`; new fixture copies are diagnostic only. Configure/build exits0.
- Final `mkdocs build --strict`: exit0,7.56s. `tools/docs_validation.py`: exit0,510Markdown+nav. `git diff --check`: exit0. Exact-worktree executable `/proc` argv0 scan:0 survivors.

Logs: `.cache/final-normal-tests-after-repair.log`, `.cache/module-build/{build-keyring,build-supervisor,build-final}.log`, `.cache/lifetime-build/build-final.log`, `.cache/mkdocs-final.log`; negative `.cache/replacement-negative.log` under detached control tree. No source changes after frozen commit.

## Fresh independent review commands (qinda only)

Use a different detached worktree at exact candidate and an ignored build directory; these commands do not install. Configured qinda MAKEOPTS was directly verified `-j24 -l24`, unchanged.

```sh
cmake -S . -B .cache/review-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr \
  -DKDE_INSTALL_LIBEXECDIR=libexec -DBUILD_TESTING=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=OFF -DQINDAQT_BUILD_SHELL=ON \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_BUILD_VIEWER=OFF \
  -DQINDAQT_BUILD_SYSTEM_MONITOR=OFF -DQINDAQT_BUILD_REMOVABLE_MEDIA=OFF \
  -DQINDAQT_BUILD_OBS_BRIDGE=OFF -DQINDAQT_BUILD_QINDALUTRIS=OFF \
  -DQINDAQT_ENABLE_AUDIO_LIVE_RUNTIME_TESTS=OFF
cmake --build .cache/review-build --target qindaqt-session \
  qindaqt_keyring_lifetime_tests qindaqt_keyring_lifetime_owner \
  qindaqt_session_supervisor_tests qindaqt_portal_session_lifetime_tests \
  -- -j24 -l24
ctest --test-dir .cache/review-build --output-on-failure -V \
  -R '^qindaqt[.](session-supervisor|session-portal-lifetime|session-child-startup|session-keyring-lifetime|session-keyring-owner-replacement)$'
mkdocs build --strict --site-dir .cache/review-docs
python3 tools/docs_validation.py --root .
```

A minimal additional source-only lifetime build is `cmake -S tests/session_supervisor/keyring_lifetime -B .cache/focused -G Ninja`, build with unchanged MAKEOPTS, then CTest. Optional `QINDAQT_KEYRING_TEST_DAEMON/COMPOSITOR/SETTINGS` paths enable real observer tests there; normal production registry derives its own built target paths.

## Bounded caveats and requested next action

Source reconnect applies when the repaired supervisor starts. It does not retrofit old current supervisor or supersede manager's current retained API caller. Tests prove private daemon/observer lifecycle and fail-closed behavior, **not live password acceptance, restored accounts, physical rendering or installed adoption**. Current desktop stays open; worker never called live AttachSession/Unlock/Prompt, paused a production process, changed config/credentials, installed software or restarted apps/services/desktop. All source, compilation/tests/heavywork occurred on qinda; laptop only orchestration and self-owned durable records.

Distinct reviewer `/root/desktop_incident_review` has immutable SHA and qinda-only instructions. Request exact review, then manager-only integration/affected gates/Portage packaging with D-067 preserved. No self-approval. Worker is available for a blocking finding repaired/rechecked against a new exact SHA, or bounded evidence/provenance help within this incident; no unrelated claim.
