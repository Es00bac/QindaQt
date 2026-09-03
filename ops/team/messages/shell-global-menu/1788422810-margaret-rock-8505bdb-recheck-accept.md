# Margaret Rock — same-reviewer active-window identity repair recheck

- Persona: **Margaret Rock**, independent compositor/security reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `8505bdbd09961863538616efd3dd8e7ef834a126`
- Tree SHA: `b1b2fac766c971ec7f2fab9fe1fd90a464cc871b`
- Parent SHA: `25baca2a01490e504ab739c852c08cb06482b6ca`
- Base SHA: `135fe652db422564b8eeb3dc85bfdf1f679c54eb`
- Rejected ancestor: `7e263bd0549d4ed64e8f15e6dac7709ccdfc7120`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/compositor-window-identity-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex`

## Findings ledger

### P0

None.

### P1

None.

### P2

None.

### P3

None.

## Recheck of prior findings

1. **P1-1 is closed.** `KWinShellWindowActionsEndpoint` now declares
   `ActiveWindowIdentityChanged` as a scriptable Qt signal and the D-Bus object
   registration exports scriptable signals. A fresh private-bus virtual-KWin
   introspection listed all six descriptor methods and
   `ActiveWindowIdentityChanged()` under signals. The live nested row also
   compares the running object's complete method/signal name sets with the
   checked-in XML. Source search found no Qt emission of that signal;
   invalidation still uses `QDBusMessage::createTargetedSignal()` for the exact
   authenticated owner, followed by revocation when panel ownership no longer
   authenticates.
2. **P1-2 is closed.** Publication rejects a candidate unless its action
   generation passes `ShellWindowGeneration::isValid()`. Decoding applies the
   same rule to the epoch and to the complete `(epoch, actionRevision)` pair.
   The original `repro_invalid_epoch` now exits `0` with
   `decoded=0 available=0 actionGenerationValid=0`; the client unit row also
   proves the invalid snapshot never becomes `identityAvailable`.
3. **P2-1 is closed.** The native Wayland live child creates a KWayland AppMenu
   on its actual Qt Wayland surface, publishes a valid service/path, and then
   drives overlong-service, malformed-service, and malformed-path transitions.
   Each hostile phase must advance to typed unavailability, and each valid
   phase must advance again and restore the exact pair. The private-bus client
   row holds an old owner's identity method reply, replaces the well-known-name
   owner, accepts the new owner's distinct epoch/PID, then sends the delayed old
   reply and requires the new snapshot to remain unchanged. These tests are
   stateful and non-vacuous.
4. **Scope is bounded.** The repair commit changes only the five owning wiki/ADR
   pages, three compositor implementation files, and focused compositor/client
   tests. Its parent records the rejected candidate's workflow handoff only.
   No JSON, production registry, applet wiring, session composition, host
   integration, or unrelated product module changed.

## Commands and results

### Immutable candidate and scope

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-parse 135fe652db422564b8eeb3dc85bfdf1f679c54eb^{commit}
git status --porcelain
git diff --name-status 25baca2a01490e504ab739c852c08cb06482b6ca..HEAD
git diff --name-only 135fe652db422564b8eeb3dc85bfdf1f679c54eb..HEAD -- '*.json'
```

Exit `0`. The candidate/tree/parent/base values match the header. Worktree
status was empty before review and immediately before writing this verdict.
The repair path list was limited to the 17 identity implementation, test, and
documentation paths described above. No JSON path was returned.

### Configure

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Both exited `0`. CMake completed configuration and generation for both exact
profiles. It repeated the known mixed-prefix runtime-search-path warnings.

### Focused and adjacent builds

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/debug \
  --parallel 3 --target qindaqt_compositor \
  qindaqt_shell_window_identity_tests \
  qindaqt_shell_window_actions_live_probe \
  qindaqt_shell_window_actions_client_tests \
  qindaqt_shell_window_actions_private_bus_tests tests/compositor/all
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/release \
  --parallel 3 --target qindaqt_compositor \
  qindaqt_shell_window_identity_tests \
  qindaqt_shell_window_actions_live_probe \
  qindaqt_shell_window_actions_client_tests \
  qindaqt_shell_window_actions_private_bus_tests tests/compositor/all
```

Both exited `0`; each incremental build reached its final `67/67` Ninja action.

### Original invalid-epoch reproduction

```sh
g++ -std=c++20 -fPIC \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/repro_invalid_epoch.cpp \
  -I /home/cabewse/work_SPaC3/container-wm-workers/compositor-window-identity-codex-review/src/compositor/include \
  $(pkg-config --cflags Qt6Core) \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/debug/src/compositor/libqindaqt_compositor_shell_actions.a \
  $(pkg-config --libs Qt6Core) \
  -o /home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/repro_invalid_epoch \
  && /home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/repro_invalid_epoch
```

Exit `0`; output:

```text
decoded=0 available=0 actionGenerationValid=0 error=active-window identity lineage is invalid
```

### Debug and Release selectors

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/debug \
  -R '^compositor\.' --output-on-failure --no-tests=error --parallel 1
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/release \
  -R '^compositor\.' --output-on-failure --no-tests=error --parallel 1
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/debug \
  -R '^qindaqt\.shell-window-actions-(client|private-bus)$' \
  --output-on-failure --no-tests=error
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/release \
  -R '^qindaqt\.shell-window-actions-(client|private-bus)$' \
  --output-on-failure --no-tests=error
```

All exited `0`: compositor Debug `49/49`, compositor Release `49/49`, client
Debug `2/2`, and client Release `2/2`. `compositor.kwin-shell-window-actions`
ran serially in both profiles and passed.

The focused verbose confirmation was:

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/debug \
  -R '^(compositor\.shell-window-identity|compositor\.kwin-shell-window-actions|qindaqt\.shell-window-actions-(client|private-bus))$' \
  -V --no-tests=error --parallel 1
```

Exit `0`, `4/4` CTest rows. The identity unit binary reported `9/9` QtTest
cases, including `rejectsInvalidActionGenerationEpochs`; the client binary
reported `11/11`, including `invalidIdentityEpochFailsClosed`; and the private
bus binary reported `7/7`, including
`identityOwnerReplacementRejectsLateOldOwnerReply`.

### Independent live introspection

The prior private-bus virtual-KWin introspection reproduction was rerun with
the Debug launcher/plugin, under scratch root
`/home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/introspect-r2`:

```sh
dbus-run-session -- bash -c '
scratch=/home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/introspect-r2
mkdir -p "$scratch/config" "$scratch/data" "$scratch/cache" "$scratch/state" "$scratch/runtime"
chmod 700 "$scratch/runtime"
unset DISPLAY WAYLAND_DISPLAY
export XDG_CONFIG_HOME="$scratch/config" XDG_DATA_HOME="$scratch/data"
export XDG_CACHE_HOME="$scratch/cache" XDG_STATE_HOME="$scratch/state"
export XDG_RUNTIME_DIR="$scratch/runtime" XDG_CURRENT_DESKTOP=QindaQt
export XDG_SESSION_DESKTOP=qindaqt KWIN_COMPOSE=Q QT_QPA_PLATFORM=wayland
export QT_QUICK_BACKEND=software
export LD_LIBRARY_PATH=/home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-arch-665/root/usr/lib
export QT_PLUGIN_PATH=/home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-arch-665/root/usr/lib/qt6/plugins
export XDG_DATA_DIRS=/home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-arch-665/root/usr/share:/usr/share
/home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/debug/src/session/qindaqt-wm \
  --plugin-root /home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/debug/plugins \
  --kwin /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-arch-665/root/usr/bin/kwin_wayland \
  --virtual --width 1280 --height 720 --scale 1 --output-count 1 \
  --no-lockscreen --no-global-shortcuts --session /usr/bin/dbus-monitor \
  >"$scratch/kwin.out" 2>"$scratch/kwin.err" &
kwin_pid=$!
for n in $(seq 1 100); do
  gdbus introspect --session --dest org.qindaqt.Compositor \
    --object-path /org/qindaqt/CompositorShell >"$scratch/introspection.txt" 2>/dev/null && break
  sleep 0.05
done
sed -n "/org.qindaqt.CompositorShell1 {/,/};/p" "$scratch/introspection.txt"
kill "$kwin_pid" 2>/dev/null || true
wait "$kwin_pid" 2>/dev/null || true
'
```

Exit `0`; observed the six XML methods and:

```text
signals:
  ActiveWindowIdentityChanged();
```

### Static gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/site
./tools/check-source-shape
git diff --check
git diff 135fe652db422564b8eeb3dc85bfdf1f679c54eb..HEAD --check
git diff 7e263bd0549d4ed64e8f15e6dac7709ccdfc7120..HEAD --check
```

All exited `0`. Documentation validation covered 125 Markdown documents and
navigation; strict MkDocs completed; source shape checked 1,956 files. Its
three threshold warnings are unchanged paths outside this repair
(`tests/compositor/CMakeLists.txt` at 500 non-blank lines and two unrelated test
files at 539/563). No JSON changed, so `python3 -m json.tool` was not
applicable.

No `tests/session` row, host system/session bus, hardware, uinput, physical
display/GPU, or network test was run. Dynamic evidence used private D-Bus and
virtual pinned KWin only.

## Verdict

ACCEPT — the two P1 defects and P2 evidence gap from `7e263bd` are closed, and
the repaired descendant has no P0–P3 finding.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
