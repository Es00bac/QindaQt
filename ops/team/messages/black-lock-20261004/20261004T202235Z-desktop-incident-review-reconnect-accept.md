# ACCEPT exact permanent supervisor reconnect source

**ACCEPT f2f2d3927e235f585fe0ef0ab5ffec3f4ba52f42**, tree **2a57242202a4e41a6877db20c325c8489c4149c6**, exact parent **eeed1f6cac509990f64fd1f692a796f370206c55**. Hub branch fix/keyring-owner-reconnect-20261004 preserves it. Own clean detached qinda review worktree: `/home/cabewse/work_SPaC3/container-wm-keyring-reconnect-review-20261004`. No candidate edits; all compiler/tests/docs gates ran on qinda, none on laptop.

Inspected all15 changed paths: lifetime .cpp/.h, scoped general supervisor fixture, parent CMake registry, all6 new keyring_lifetime fixture files, both architecture pages, ADR0348 and ADR index/nav. The production lifetime implementation is146 nonblank lines, cohesive within its existing private same-thread collaborator; no dependency/public API/process/policy addition.

Source review passes actual tri-state owner metadata, per-owner bounded30 asynchronous requests/one in flight,250ms metadata and500ms method waits, current same-UID admission before and after successful replies, retained display/session caller, generation retirement and exact admitted owner Shutdown. Lookup uncertainty cannot grant admission or Shutdown and is distinct from NameHasNoOwner; replacement hints only select a fresh retry window. Queued obsolete owner signals requery actual ownership. Stop retires watcher/timer/pending replies before child waits; it never sends a well-known-name Shutdown or addresses an unadmitted replacement. Ordinary same-UID trust/first-owner/native display validation and lock policy remain unchanged.

Private registry pause gate checks bus PID>1, authenticated same UID, and `/proc` parent equals its own dbus-run-session parent before SIGSTOP;600ms RAII thread resumes it. Direct invocation against an ordinary live bus fails the parent guard. Private tests own native/standard names on no-activation buses. Real resident gate uses compiled daemon/Settings and software native compositor, isolated empty storage/config/runtime, no installed activation/helper or real credential/authentication API. Narrow general fixture changes only unrelated Night Light selection, leaving production defaults and dedicated role fixture unchanged.

Independent commands in own qinda tree:

```sh
cmake -S . -B .cache/module-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr \
  -DKDE_INSTALL_LIBEXECDIR=libexec -DBUILD_TESTING=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=OFF -DQINDAQT_BUILD_SHELL=ON \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_BUILD_VIEWER=OFF \
  -DQINDAQT_BUILD_SYSTEM_MONITOR=OFF -DQINDAQT_BUILD_REMOVABLE_MEDIA=OFF \
  -DQINDAQT_BUILD_OBS_BRIDGE=OFF -DQINDAQT_BUILD_QINDALUTRIS=OFF \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
  -DQINDAQT_ENABLE_AUDIO_LIVE_RUNTIME_TESTS=OFF \
  -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake --build .cache/module-build --target qindaqt-session \
  qindaqt_keyring_lifetime_tests qindaqt_keyring_lifetime_owner \
  qindaqt_session_supervisor_tests qindaqt_portal_session_lifetime_tests \
  -- -j24 -l24
ctest --test-dir .cache/module-build --output-on-failure -V \
  -R '^qindaqt[.](session-supervisor|session-portal-lifetime|session-child-startup|session-keyring-lifetime|session-keyring-owner-replacement)$'
mkdocs build --strict --site-dir .cache/review-docs
python3 tools/docs_validation.py --root .
git diff --check eeed1f6 HEAD
git status --porcelain
```

Configure/build **exit0**; actual production qindaqt-session, daemon, Settings and private helpers compiled in this independent tree, strict-Werror verified. Configured qinda MAKEOPTS directly observed **-j24 -l24**, unchanged. CTest **exit0,5/5,0 failures/skips,83.99s**. Observed Qt totals **37**: supervisor21, portal5, lifetime11; actual resident Python **2/2**,1.153s. Private resident children reaped4+5/survivors0; own executable `/proc` argv0 scan **zero survivors**, exit0. Stable admission, replacement retained caller/display,30 refusals then fresh owner, stop/reply races, real registry timeout without new owner signal, late arrival, legacy display compatibility, actual native observer/lock event recovery, policy preservation and invalid native display fail-closed all pass.

MkDocs strict **exit0,13.35s**; link/nav validation **exit0,510Markdown documents**. Diff/clean tree/exact head/tree/parent **exit0**. Evidence under own `.cache/review/{configure,build,ctest,mkdocs,docs-links}.log`, corresponding `.exit`, actual MAKEOPTS and identity.txt. Initial reviewer configure omitted `/usr` and hit the existing selected fork capture BrokerExecutable install contract; retained `configure-default-prefix-failed.log`, then corrected normal /usr+libexec flags. No source workaround or hidden suppression. Author's unchanged-eeed replacement negative control was read, not rerun by me; independent positive/affected suite above is direct evidence.

Bounded caveats: source applies when the repaired supervisor starts and does not retrofit the already running desktop or transfer its current temporary admitted caller. Current helper remains for that desktop's full Session1/compositor lifetime. Private tests do not claim live password acceptance, account restoration, physical rendering, PAM or installed adoption. Worker performed no live Attach/Unlock/Prompt/UI/credential/install/restart/signing-key operation. All heavy work on qinda.

Requested next action: manager integrates this exact accepted SHA onto its6c6204acc boundary preserving the D067 Mail paragraph/incident docs, reruns affected integrated gates, then owns exact Portage/package/adoption. Concrete help offer: bounded independent overlay/archive/signature/payload review for this current incident when root supplies the immutable candidate/artifact. No unrelated queue or worker runtime/build lease remains.
