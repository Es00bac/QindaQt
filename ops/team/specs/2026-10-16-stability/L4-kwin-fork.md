# L4 — Compositor fork robustness (qindaqt-kwin, plus the capture backend in container-wm) — Opus

Bases:
- qindaqt-kwin hub `main` (`24d0c6a6fe`, the installed r6). Branch `stability/l4-fork-20261016`. Worktree
  `~/work_SPaC3/qindaqt-kwin.worktrees/stab-l4-fork-20261016`.
- For the capture backend only: a container-wm worktree `~/work_SPaC3/container-wm.worktrees/stab-l4-capture-20261016`
  from the container-wm base named in the assignment, branch `stability/l4-capture-20261016`.

Voice seat: `claude-helper-four` (create it if missing). Read qindaqt-kwin's `AGENTS.md` and `qindaqt/README.md`
first. The identity rules, the `qindaqt:` commit prefix and the `QindaQt-ADR:` trailer apply.

## Why
- **Network-share mounts.** For permission checks the fork scans every installed application and calls
  `QFileInfo(exec).canonicalFilePath()` on the main thread (`src/utils/serviceutils.h`
  `fetchProcessServiceField`). The owner's `origin-of-species.desktop` points through `~/tmp-wow-bits`, a
  symlink into the `~/Network/Void` sshfs automount. qindaqt-kwin triggered that automount 184 times since
  10-13. Each mount stalls the compositor about 1 s; if qinda is unreachable it can stall for 20 s.
- **Display-power polling.** `qindaqt/display-power/displaypowerauthority.cpp` makes three synchronous bus calls
  every 250 ms on the main thread.
- **Dead capture broker.** It is launched once at startup, `m_restartCount` is never reset, and launches are capped at
  3 per compositor lifetime (`qindaqt/capture-authority/captureauthority.cpp` 120–161). The backend exits
  status 2 silently (container-wm `src/services/portal/capture/backend/main.cpp` 23–31). The broker is dead
  this session: ScreenCast `AvailableSourceTypes=0`, and OBS has no PipeWire screen capture.
- **Black screen after lock-screen exit.** If `qindaqt-lock` exits while locked, nothing relaunches it
  (`lockerlauncher.cpp` 28–30, `sessionlockcontroller.cpp` 166–181), so the screen stays black. Teardown also
  waits up to 1 s on the main thread (`lockerlauncher.cpp` 34–39, `capturechild.cpp` 258–261).
- **No core dumps.** `src/main_wayland.cpp:327` sets `RLIMIT_CORE` to {0,0} in the compositor itself. The
  hard limit is inherited by Xwayland, qindaqt-session, the shell and every application, so no crash in the
  session leaves a core.
- Findings items 5, 7, 8, 13 and 16.

## Owned paths
- fork: `qindaqt/**`, `src/utils/serviceutils.h` and its tests, `src/main_wayland.cpp` (the core-limit lines
  only), `src/wayland_server.cpp` (only if the permission filter needs a cache hook)
- container-wm: `src/services/portal/capture/**` (logging and exit reasons only; do not change the protocol)

## Outcomes
1. **Permission lookups without filesystem walks.**
   - Compare the client executable's basename against each entry's `Exec` basename before any filesystem call.
   - Canonicalize only candidates, never paths on network, FUSE or autofs mounts (check with `statfs`, or keep
     to trusted system prefixes).
   - Cache the result per canonical executable path, and invalidate when the service database changes.
   - Add a regression test with a desktop entry whose `Exec` points into an unreachable or slow path; the
     check must not touch it.
2. **Display power.** Replace the 250 ms poll with a QDBusServiceWatcher and cached credentials. No synchronous
   bus calls on the main thread in steady state.
3. **Capture broker lifecycle.**
   - Reset the restart budget after a successful publish.
   - Use bounded exponential backoff (for example 2 s doubling to 60 s), so it never gives up permanently while
     the compositor runs.
   - Relaunch when the portal frontend or backend owner appears again.
   - `selectedCompositor()` and actor lookups use cached, watcher-maintained state rather than three blocking
     calls each time.
   - Every broker exit path in the backend logs a clear reason to stderr before returning.
4. **Locker supervision.**
   - If the locker exits unexpectedly while the session is locked, relaunch it with backoff (for example 0.5 s
     doubling to 8 s, unlimited while locked) and keep the screen protected.
   - Log each exit's status.
   - Replace main-thread blocking waits in the locker and capture-child teardown with asynchronous handling.
5. **Core limit.**
   - The compositor stays non-dumpable through `PR_SET_DUMPABLE(0)`, which already prevents its own core
     dumps, but it must not lower the hard `RLIMIT_CORE` inherited by its children.
   - The locker and capture children keep setting their own limits.
   - Record the security reasoning in an AGENT-NOTE and the ADR, and update the ADR referenced by the
     existing AGENT-GUARD.

## Acceptance
- Fork:
  - extend the service-permissions tests (`qindaqt/service-permissions/tests`) for the basename prefilter,
    the cache, and the unreachable path not being touched;
  - lock-launcher tests for relaunch-after-exit with a fake locker;
  - capture-authority tests for budget reset, backoff and relaunch on owner return;
  - verify the core limit in a child-process test.
- Build per `qindaqt/README.md` (Release, `BUILD_TESTING=OFF`, plus `-DQINDAQT_KWIN_BUILD_TESTS=ON` for
  helper tests).
- `qindaqt/tools/rename-identity --check` passes.
- container-wm: a focused build of the capture backend.
- Report everything with counts.

## Reference designs (study, don't depend)
- KDE KScreenLocker: greeter crash relaunch with a fallback.
- KWin upstream: permission checks via `X-KDE-Wayland-Interfaces`.
- xdg-desktop-portal-gnome / mutter: screencast session lifecycle.
