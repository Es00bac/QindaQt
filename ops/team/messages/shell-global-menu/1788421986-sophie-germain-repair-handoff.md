# Sophie Germain handoff — active-window identity exact repair

- Handoff time: `2026-09-03T01:53:06-06:00`
- Exact candidate commit: `8505bdbd09961863538616efd3dd8e7ef834a126`
- Candidate tree: `b1b2fac766c971ec7f2fab9fe1fd90a464cc871b`
- Exact feature base: `135fe652db422564b8eeb3dc85bfdf1f679c54eb`
- Rejected product ancestor: `7e263bd0549d4ed64e8f15e6dac7709ccdfc7120`
- Exact repair starting tip / candidate parent: `25baca2a01490e504ab739c852c08cb06482b6ca`
- Branch: `worker/compositor-window-identity`
- Feature: QQ-004 Global Menu compositor active-window identity facts

## Changed paths in the repair commit

- `docs/wiki/adr/0062-project-authenticated-active-window-identity.md`
- `docs/wiki/architecture/compositor-session.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/compositor-control-v1.md`
- `docs/wiki/shell/global-menu.md`
- `src/compositor/kwin/kwinshellwindowactions.h`
- `src/compositor/kwin/qindaqtkwinplugin.cpp`
- `src/compositor/src/shellwindowidentity.cpp`
- `tests/compositor/ShellWindowActionsTests.cmake`
- `tests/compositor/shellwindowactionsliveclients.cpp`
- `tests/compositor/shellwindowactionsliveclients.h`
- `tests/compositor/shellwindowactionslivecontract.cpp`
- `tests/compositor/shellwindowactionslivecontract.h`
- `tests/compositor/shellwindowactionsliveprobe.cpp`
- `tests/compositor/tst_shellwindowidentity.cpp`
- `tests/shell_window_actions_client/tst_shellwindowactionsclient.cpp`
- `tests/shell_window_actions_client/tst_shellwindowactionsprivatebus.cpp`

## Outcome

The live `CompositorShell1` object now exports the descriptor-declared
`ActiveWindowIdentityChanged` signal and its complete method/signal set is
compared at runtime with the immutable XML on the private KWin bus. The Qt
signal remains deliberately un-emitted: identity invalidation still uses the
same member name in a targeted D-Bus message sent only to the authenticated
shell owner.

Identity publication and decoding apply the public
`ShellWindowGeneration::isValid()` rule. A padded or otherwise invalid epoch
cannot become available client truth. The registered deterministic compositor
and client cases cover the reviewer reproduction, and the original standalone
reproduction now exits successfully with decoder rejection.

The nested private-KWin proof uses KWayland on the child's actual Qt Wayland
connection to announce a valid AppMenu service/path, observes the exact pair,
then proves typed withdrawal and later recovery for an overlong service, a
malformed service, and a malformed object path. The private-bus client row now
holds an old owner's identity reply across service replacement and proves that
the late reply cannot replace the accepted new-owner snapshot.

## Acceptance evidence

All final acceptance commands ran from the lane worktree. `<ROOT>` is
`/home/cabewse/work_SPaC3/builds/qindaqt/compositor-window-identity`.

- Debug configure: `cmake -S . -B <ROOT>/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON`; exit `0`.
- Release configure: the same command with `-B <ROOT>/release -DCMAKE_BUILD_TYPE=Release`; exit `0`.
- Debug focused/adjacent build: `cmake --build <ROOT>/debug --parallel 3 --target qindaqt_compositor qindaqt_shell_window_identity_tests qindaqt_shell_window_actions_live_probe qindaqt_shell_window_actions_client_tests qindaqt_shell_window_actions_private_bus_tests tests/compositor/all`; exit `0`.
- Release focused/adjacent build: `cmake --build <ROOT>/release --parallel 3 --target qindaqt_compositor qindaqt_shell_window_identity_tests qindaqt_shell_window_actions_live_probe qindaqt_shell_window_actions_client_tests qindaqt_shell_window_actions_private_bus_tests tests/compositor/all`; exit `0`.
- Final post-cleanup live-probe builds in Debug and Release: `cmake --build <ROOT>/<profile> --parallel 3 --target qindaqt_shell_window_actions_live_probe`; both exit `0`.
- Debug compositor: `ctest --test-dir <ROOT>/debug -R '^compositor\.' --output-on-failure --no-tests=error --parallel 1`; exit `0`, 49/49 passed.
- Release compositor: `ctest --test-dir <ROOT>/release -R '^compositor\.' --output-on-failure --no-tests=error --parallel 1`; exit `0`, 49/49 passed.
- Debug client: `ctest --test-dir <ROOT>/debug -R '^qindaqt\.shell-window-actions-(client|private-bus)$' --output-on-failure --no-tests=error`; exit `0`, 2/2 passed.
- Release client: the same selector in `<ROOT>/release`; exit `0`, 2/2 passed.
- The private-KWin `compositor.kwin-shell-window-actions` row ran serially in both broad selectors and passed. A focused final Debug invocation also passed 1/1.
- Margaret Rock's reproduction was compiled against this candidate's headers and Debug library under `<ROOT>` and run: `g++ -std=c++20 -fPIC /home/cabewse/work_SPaC3/builds/qindaqt/review-identity-codex/repro_invalid_epoch.cpp -I <worktree>/src/compositor/include $(pkg-config --cflags Qt6Core) <ROOT>/debug/src/compositor/libqindaqt_compositor_shell_actions.a $(pkg-config --libs Qt6Core) -o <ROOT>/repro_invalid_epoch && <ROOT>/repro_invalid_epoch`; exit `0`, reporting `decoded=0 available=0 actionGenerationValid=0`.
- `./tools/validate-docs`; exit `0`, 125 Markdown documents and navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site`; exit `0`.
- `./tools/check-source-shape`; exit `0`, 1,956 files checked with only the three reported pre-existing threshold warnings; the expanded live probe has 482 non-blank lines and remains below the required limit.
- `git diff --check`, `git diff 135fe652db422564b8eeb3dc85bfdf1f679c54eb..HEAD --check`, and `git diff --cached --check`; exit `0` at their respective pre-commit gates.
- No JSON changed, so the JSON parse gate was not applicable.

Repair-loop evidence is explicit. The first strict live-contract compile failed
because a test member named `signals` collided with Qt's macro; it was renamed
to `signalNames`. The expanded nested proof then failed twice while attempting
an overlong object path. Wayland rejects that transport-sized string before it
reaches KWin, so the final honest adapter matrix retains the overlong service
and malformed service/path cases that cross the real boundary. All final rows
above pass.

## Bounded caveats

- This candidate repairs the compositor/client proof boundary. It does not
  instantiate the separately owned Global Menu G2 composition or applet.
- A Wayland object-path value larger than the identity contract's 4,096-byte
  ceiling also exceeds the Wayland string transport ceiling and cannot reach
  KWin through this protocol. The live row therefore proves an overlong service
  and malformed service/path; deterministic codec tests retain the full bounds.
- The standard AppMenu registrar remains unauthenticated. Composition must
  still resolve provider ownership and compare credentials around a stable
  identity revision, as ADR-0062 requires.
- No host desktop, host D-Bus service, `tests/session`, hardware, uinput,
  physical display/GPU, or network product test was run. Live evidence used a
  private bus and virtual pinned KWin 6.6.5 under the assigned build root.

## Requested next action

Margaret Rock (Codex) should independently review exact candidate
`8505bdbd09961863538616efd3dd8e7ef834a126`; if accepted, the Program Manager
should integrate it.
