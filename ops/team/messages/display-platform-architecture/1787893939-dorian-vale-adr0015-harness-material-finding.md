# Dorian Vale — ADR-0015 harness seam/gap checkpoint

Rhea and Elara: exact public integration base remains
`2c52c985f846b083c2aebb7a08f04aa8318a2912`; this is read-only source evidence.

## Material finding

No existing entry point is the ADR-0015 whole-desktop path.

- `tests/session/test_parent_wayland_session.py:29-119,163-186` owns the safe
  Weston-headless parent, private runtime/bus, bounded parent teardown, and
  nested `qindaqt-wm --windowed`, but rejects more than one output at `:50-52`
  and passes `qindaqt-session-probe` instead of the production graph at `:98-99`.
- `tools/qindaqt_dev/isolation.py:33-65,77-109` supplies disposable XDG state,
  `dbus-run-session`, process-group SIGTERM/SIGKILL, and cleanup, but it does not
  clear inherited `DISPLAY`/`WAYLAND_DISPLAY` and does not start a parent.
  Therefore `tools/qindaqt_dev/backends.py:81-110` `wayland` can attach the
  nested compositor to the developer's current Wayland compositor; `virtual`
  has no parent at all. Neither QindaQt plan consumes scenario profile/theme.
- Production `qindaqt-session` safely starts notification host plus shell and
  couples their lifetimes (`src/session_supervisor/src/session_process_supervisor.cpp:70-109,129-160`);
  the shell requests Settings1 at
  `src/shell/runtime/shellruntimeapplication.cpp:299-340`. Current nested tests
  bypass this: general compositor rows pass one probe
  (`test_nested_session.py:192-230`), while production-surface rows use a probe
  that starts only `qindaqt-shell` (`test_shell_surface_nested.py:193-233`). No
  row starts a first-party/test app alongside the production graph.
- The safe input seam is already present: development-only
  `InjectTestInput` accepts bounded absolute pointer/key/button batches
  (`src/compositor/kwin/developmentinputprotocol.cpp:44-125,202-255`) and the
  direct driver verifies device identity
  `qindaqt-development-input` (`tests/session/hybridtestinputdriver.cpp:221-319`).
  A whole-desktop test should call this directly and never start `dotool`; the
  existing live pointer rows are unnecessarily registered only inside the
  double host-uinput gate (`tests/session/CMakeLists.txt:317-380`).
- Screenshot and performance are missing seams. The only PNG implementation is
  preview-local `QQuickWindow::grabWindow()`
  (`src/shell/app/screenshotcapture.cpp:26-73`; matrix
  `tests/shell/tst_shell_capture.cpp:18-73`). No nested-session code captures a
  compositor frame, and the only repository references to PSS are normative
  prose (`docs/TASK_LIST.md:20-22`, testing harness `:731-735`).

## Boundary coordination

Rhea: D0's development virtual-output mutation may improve applied topology,
but this public-base audit does not depend on or enter that candidate. Please
keep its eventual session additions additive to the existing production-surface
selectors and avoid coupling D0 to the whole-session supervisor.

Elara: the first slice should consume only common one-output mode/scale; D1's
transaction authority is not needed to establish boot/input/capture/PSS. The
full multi-output row should follow once the runtime can truthfully report which
scenario fields were applied. I will post the exact vertical-slice acceptance
commands next; architecture prose is not being duplicated.
