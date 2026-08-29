# Ada Ruiz — Settings1 daemon-loss repair handoff

- **Timestamp:** 2026-08-27T12:56:23-06:00
- **Preserved rejected candidate:** `2a1e2626e5d4e8e4526bfadbb8100931208f3179`
- **Repair candidate:** `3de6bfae911594804e00a913f2feef5f1b36e16e`
- **Branch/worktree:** `worker/ada-settings1` at
  `/home/cabewse/work_SPaC3/container-wm-workers/ada-settings1`
- **Tree:** clean
- **Repair size:** 6 paths; 396 insertions; 1 deletion

## Repair outcome

This one new imperative commit repairs the sole P2 in final release review
`1787855945`/`1787856125` without amending `2a1e262` or changing any accepted
Settings1 wire, repository, persistence, client, controller, or UI behavior:

1. Production obtains one `QDBusConnection::sessionBus()` handle, subscribes
   that exact constructing connection to the standard local
   `/org/freedesktop/DBus/Local`, `org.freedesktop.DBus.Local.Disconnected`
   signal, and routes it to `QCoreApplication::quit()` before service startup.
   Failure to install that lifetime observation fails startup rather than
   creating an unobservable resident.
2. The adjacent `AGENT-GUARD` and durable architecture/ADR text state the
   invariant: one activated process owns one daemon/owner/epoch lineage. It
   must exit on permanent connection loss; reconnecting a stale in-memory
   repository is forbidden. Replacement activation constructs fresh authority.
3. A new, separate 301-nonblank-line process-lifecycle test uses the real
   production executable, real D-Bus activation, and two successive private
   daemons. It records the exact PID/owner/epoch, terminates each daemon, and
   requires the activated PID to disappear within five seconds. Replacement
   activation must produce a different live PID, owner slot, and epoch.
4. Each activation also performs the proper UnknownKey set probe: exact wire
   status 7, same epoch, revisions 0→0, empty values/sources/changed keys,
   bounded key-specific diagnostic, and no user file. This preserves the
   immediately prior repair at the production-process boundary.
5. An exact-executable RAII guard terminates only a matching fixture if any
   assertion fails, so the test cannot leave an orphan. Explicit environment
   overrides let the identical regression target an installed prefix's real
   descriptor, executable, and schemas.

## Changed paths

- `src/services/settings_service/src/main.cpp`
- `tests/services/settings_service/CMakeLists.txt`
- `tests/services/settings_service/tst_settings_service_process_lifecycle.cpp`
- `docs/wiki/architecture/settings-service.md`
- `docs/wiki/adr/0012-persist-notification-quieting-through-settings1.md`
- `docs/wiki/development/testing-harness.md`

## Exact-source verification

- Debug build — exit 0.
- Debug `^qindaqt\.settings-` selection — exit 0, **16/16 passed**.
- Debug full `^qindaqt\.` selection — exit 0, **71/71 passed**.
- Release build — exit 0.
- Release `^qindaqt\.settings-` selection — exit 0, **16/16 passed**.
- Release full `^qindaqt\.` selection — exit 0, **71/71 passed**.
- Exact committed process-lifecycle test with `--repeat until-fail:10` —
  **10/10 passed in Debug** and **10/10 passed in Release**; individual runs
  completed in 0.07–0.15 seconds.
- Production Release build and `all_qmllint` — exit 0. Only established
  unrelated shell-preview warnings were emitted; changed code has no QML.
- `tools/validate-docs` — exit 0, **42 Markdown documents** and navigation.
- strict MkDocs — exit 0.
- `tools/check-source-shape --warnings-as-errors --largest 30` — exit 0,
  **769 files**, zero skips/violations. New test: 301 nonblank lines.
- Working, cached, commit, and exact `HEAD^` diff whitespace checks — exit 0.
- Production install to isolated `build/ada-stage-prefix` — exit 0. The actual
  installed activation descriptor names the actual staged executable.
- The committed Debug process harness was repointed to the installed prefix's
  descriptor, Release service binary, and installed schemas — exit 0,
  **3/3 QtTest phases passed in 130 ms**. It activated twice across two real
  private daemons, passed the exact UnknownKey probe on both owners, observed
  both exact PIDs disappear after daemon termination, and found no user file.
- Final anchored process query found no staged/Debug/Release Settings1 service
  fixture. Final product `git status --short --branch` is clean at exact
  `3de6bfae911594804e00a913f2feef5f1b36e16e`.

## Scope and requested next action

No live desktop, user session bus, compositor, KGlobalAccel, uinput, pointer,
keyboard, integration checkout, reviewer worktree, Mira worktree, or unrelated
native/platform/display lane was used or modified. The QST-1 integration
preflight and installed-consumer finding were read; they do not change this
accepted no-scope-growth Settings1 boundary.

Please return exact candidate
`3de6bfae911594804e00a913f2feef5f1b36e16e` to the same final release reviewer
for recheck against `1787855945` and `1787856125`. The reviewer should reproduce
installed activation, kill the real private daemon without explicitly stopping
the service, require the exact process to disappear promptly, activate on a
replacement daemon, require fresh process/owner/epoch plus the exact UnknownKey
outcome, and confirm no fixture remains. Only that exact-hash recheck should
decide integration.
