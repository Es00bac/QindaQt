# Margaret Rock — independent compositor/security exact-candidate review

- Persona: **Margaret Rock**, independent compositor/security reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `7e263bd0549d4ed64e8f15e6dac7709ccdfc7120`
- Tree SHA: `7c71661218b0e3fd10c2f9d3f76f6f95d39ffcfe`
- Parent SHA: `135fe652db422564b8eeb3dc85bfdf1f679c54eb`
- Base SHA: `135fe652db422564b8eeb3dc85bfdf1f679c54eb`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/compositor-window-identity-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex`

## Findings ledger

### P0

None.

### P1

#### P1-1 — the live D-Bus object omits its declared identity invalidation signal

The checked-in contract declares `ActiveWindowIdentityChanged`, but the live
`org.qindaqt.CompositorShell1` introspection has an empty signal set. The
endpoint has no Qt signal declaration at
`src/compositor/kwin/kwinshellwindowactions.h:83`, and the shell object is
registered with `ExportScriptableSlots` only at
`src/compositor/kwin/qindaqtkwinplugin.cpp:141`. The implementation manually
sends a targeted signal, so a client that already knows the undocumented live
shape can receive it, but the exported interface does not match the immutable
descriptor at `compositor/dbus/org.qindaqt.CompositorShell1.xml:38`.

Reproduction, on a private `dbus-run-session` and candidate-built virtual KWin:

```sh
dbus-run-session -- bash -c '
scratch=/home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/introspect
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
sed -n "20,28p" "$scratch/introspection.txt"
kill "$kwin_pid" 2>/dev/null || true
wait "$kwin_pid" 2>/dev/null || true
'
```

Exit `0`; observed:

```text
ActiveWindowIdentity(out ay arg_0);
signals:
properties:
```

Expected: `ActiveWindowIdentityChanged()` appears under the live interface's
signals, matching the shipped XML. `tests/compositor/test_dbus_contract.py:184`
only parses the checked-in XML, so its passing result is tautological with
respect to live object parity.

#### P1-2 — malformed identity epochs are published as available

`decodeShellWindowIdentitySnapshot()` checks only nonempty/maximum length for
the epoch at `src/compositor/src/shellwindowidentity.cpp:370`; it does not apply
the public `ShellWindowGeneration::isValid()` rule. It then constructs an
invalid action generation at line 440. The client accepts that result and stores
it as current truth at `src/shell_window_actions_client/src/shell_window_actions_client.cpp:236-257`.
Consequently `identityAvailable()` is true while the snapshot's required action
fence is invalid.

Reproduction source is under the assigned build root at
`/home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/repro_invalid_epoch.cpp`.

```sh
g++ -std=c++20 -fPIC \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/repro_invalid_epoch.cpp \
  -I /home/cabewse/work_SPaC3/container-wm-workers/compositor-window-identity-codex-review/src/compositor/include \
  $(pkg-config --cflags Qt6Core) \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/debug/src/compositor/libqindaqt_compositor_shell_actions.a \
  $(pkg-config --libs Qt6Core) \
  -o /home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/repro_invalid_epoch
/home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/repro_invalid_epoch
```

Exit `1`; observed:

```text
decoded=1 available=1 actionGenerationValid=0 error=
```

Expected: decoder rejection (`decoded=0`) and no available client identity.
This violates the strict wire-decoding and action-generation fencing contract.

### P2

#### P2-1 — required adversarial identity tests do not exercise two claimed boundaries

The live child at `tests/compositor/shellwindowactionsliveclients.cpp:66-84`
only maps a `QWindow`; it never announces a KDE AppMenu address or X11 AppMenu
properties. The live assertion at
`tests/compositor/shellwindowactionsliveprobe.cpp:391-418` checks only UUID,
PID, and numeric X11 ID. Therefore neither a valid Wayland service/path mapping
nor fail-closed overlong/bad service/path behavior crosses the real KWin adapter.
The candidate's own handoff accurately calls this out as a caveat, but the
review brief requires these hostile live announcements.

The identity owner-change unit row is also not a late-old-reply test:
`tests/shell_window_actions_client/tst_shellwindowactionsclient.cpp:224-245`
uses `FakeTransport::identityReply()`, which always replies to the last request
(the new owner). The private-bus identity row at
`tests/shell_window_actions_client/tst_shellwindowactionsprivatebus.cpp:240-264`
only tests one-owner invalidation/refresh. Thus identity-specific old-owner late
reply rejection is implemented by the token/owner check but lacks the required
negative control on a real private bus.

Reproduction:

```sh
rg -n 'setAddress|_KDE_NET_WM_APPMENU|appMenuServiceName|appMenuObjectPath' \
  tests/compositor/shellwindowactionsliveclients.cpp \
  tests/compositor/shellwindowactionsliveprobe.cpp
```

Exit `1`, no matches. Expected: live client announcement plus valid and hostile
service/path assertions. Inspection of the two client tests above shows no old
identity reply emitted with the replaced unique owner.

### P3

None.

## Review-question answers

1. **Identity truth:** source inspection and pinned-upstream inspection confirm
   native PID comes from `surface->client()->processId()`, XWayland PID from
   XRes `LOCAL_CLIENT_PID`, and X11 ID from `X11Window::window()`. The candidate
   takes service/path from KWin's `applicationMenu*` state and represents
   missing facts as null. Store validation fails closed for malformed pairs.
   However, the required live valid/hostile AppMenu announcement proof is absent
   (P2-1).
2. **Authentication and fencing:** the controller authenticates the D-Bus
   unique-name PID against the sole committed dock PID before touching the
   identity source; unauthorized replies are fixed and fact-free. Invalidation
   is manually unicast and the client filters by exact owner/token. The live
   interface contract omits the declared signal (P1-1), and malformed epochs
   can still be published as available (P1-2).
3. **Threat model honesty:** ADR-0062, the reference page, and the Global Menu
   consumption contract correctly state that registrar claims remain
   unauthenticated, bus-name ownership must be resolved separately, compromised
   same-process applications remain able to export their own hostile menu, and
   delegated exporters with another PID fail closed.
4. **Client:** the existing window-actions transport owns identity refresh; no
   second compositor client is introduced. Owner/token/revision checks and
   dirty-read coalescing exist. P1-2 violates strict lineage validity, and the
   identity-specific late-old-owner case is not proven (P2-1).
5. **Tests:** both profiles pass all 49 compositor rows and both client rows,
   including the serial private-virtual-KWin row. The XML contract test passes,
   but it does not compare live introspection and missed P1-1. The live and
   late-reply omissions are detailed in P2-1.

## Commands and results

Initial and final identity checks:

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-parse 135fe65^{commit}
git status --porcelain
```

Exit `0`. Candidate/tree/parent/base match the header; status was empty before
and after review.

Configurations (run once for each profile):

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

Both exited `0`.

Focused builds, in Debug and Release:

```sh
cmake --build <ROOT>/<profile> --parallel 3 --target \
  qindaqt_compositor qindaqt_shell_window_identity_tests \
  qindaqt_shell_window_actions_live_probe \
  qindaqt_shell_window_actions_client_tests \
  qindaqt_shell_window_actions_private_bus_tests
```

Both exited `0`, 176/176 Ninja steps.

Adjacent compositor builds:

```sh
cmake --build <ROOT>/debug --parallel 3 --target tests/compositor/all
cmake --build <ROOT>/release --parallel 3 --target tests/compositor/all
```

Both exited `0`, 265/265 Ninja steps.

Selectors:

```sh
ctest --test-dir <ROOT>/debug -R '^compositor\.' --output-on-failure --no-tests=error --parallel 1
ctest --test-dir <ROOT>/release -R '^compositor\.' --output-on-failure --no-tests=error --parallel 1
ctest --test-dir <ROOT>/debug -R '^qindaqt\.shell-window-actions-(client|private-bus)$' --output-on-failure --no-tests=error
ctest --test-dir <ROOT>/release -R '^qindaqt\.shell-window-actions-(client|private-bus)$' --output-on-failure --no-tests=error
```

All exited `0`: compositor Debug 49/49, compositor Release 49/49, client Debug
2/2, client Release 2/2. The nested row ran serially in both compositor
selectors and passed.

Static gates:

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/site
./tools/check-source-shape
git diff --check
git diff 135fe65..HEAD --check
```

All exited `0`. Documentation validated 125 Markdown documents and navigation;
MkDocs strict completed; source shape checked 1,954 files with only the three
reported pre-existing threshold warnings. No JSON file changed, so the JSON
parse gate was not applicable.

Pinned-upstream credential evidence:

```sh
tar -xOf /var/cache/distfiles/kwin-6.6.5.tar.xz kwin-6.6.5/src/x11window.cpp | rg -n -C 8 'm_pid|X11Window::pid|LOCAL_CLIENT'
tar -xOf /var/cache/distfiles/kwin-6.6.5.tar.xz kwin-6.6.5/src/waylandwindow.cpp | rg -n -C 8 'm_pid|WaylandWindow::pid'
```

Exit `0`; X11 PID is populated by `xcb_res_query_client_ids` with
`XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID`, and Wayland PID is initialized from
`surface->client()->processId()`.

No host session/system bus, hardware, uinput, network, or `tests/session` row
was run. Dynamic compositor reproductions used a private bus and virtual KWin
under the assigned build root.

## Verdict

REJECT — two P1 contract/fail-closed defects and one P2 acceptance-evidence gap.

VERDICT REJECT P0/P1/P2/P3=0/2/1/0
