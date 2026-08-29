# Church the 2nd — Terminal S0 exact-repair rereview FAIL

- Time: 2026-08-28T08:51:37-06:00
- Reviewer: Church the 2nd (OpenAI collaboration runtime; exact serving model
  and reasoning unexposed)
- Addressees: Micah Stone; Program Manager
- Exact candidate: `2386e7464bcebe17dd074299ac20f1739a5bf8b1`
- Tree: `e263cdd265aa2f722b7d9277dbd61d1593f258e4`
- Parent: `f98d0e194e387bc63d7860de61ff760cf3ec2166`
- Original base / merge base with current public:
  `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- Current public inspected: `50742fed62427c2f848ac13df94c488366e136a0`
- Verdict: **FAIL — P0/P1/P2/P3 = 0/4/5/4**

This verdict applies only to immutable commit `2386e74`. I verified the
detached worktree remained clean, independently reproduced the ADR-renumber
manifest SHA-256 `cc70fe78a5a79532f7d3f9ea4a003e5738af9e6a100cec3ade371f27c7a45be9`,
read every Terminal source/test/doc/build path and the governing AppShell,
module, testing, documentation and team policies, and checked pinned upstream
qtermwidget 2.4.0 source where its private PTY direction and selection bounds
decide correctness. No product/Git mutation, compiler, registered CTest, PTY,
GUI, session, host input/config, or live-runtime action occurred.

## P1-1 — Keyboard/paste data goes in the wrong PTY direction

The adapter duplicates qtermwidget's **slave** at
`src/apps/terminal/ui/terminal_widget_adapter.cpp:126-139`, receives emulator
keyboard bytes at lines 143-146, and writes them back to that slave at lines
267-309. The child reads stdin from the same slave (`69-85,229-239`). PTY input
must be written to the master; a slave write is output toward the master and
emulator. Consequently typing, paste, and `sendTextToSession()` do not reach
the shell. The same code applies `O_NONBLOCK` to the duplicated slave at lines
131-134; because duplicated descriptors share file status flags, the child's
dup2'd stdin/stdout/stderr also become nonblocking.

Pinned upstream confirms `startTerminalTeletype()` disconnects emulator input
from its private master writer and exposes `sendData`, while
`getPtySlaveFd()` returns the actual slave:
[qtermwidget.cpp](https://github.com/lxqt/qtermwidget/blob/4ca192e/lib/qtermwidget.cpp#L280-L290),
[Session.cpp](https://github.com/lxqt/qtermwidget/blob/4ca192e/lib/Session.cpp#L80-L83),
[Session.cpp](https://github.com/lxqt/qtermwidget/blob/4ca192e/lib/Session.cpp#L341-L353),
[Pty.cpp](https://github.com/lxqt/qtermwidget/blob/4ca192e/lib/Pty.cpp#L333-L343).
This is not repairable by buffering more slave writes. A viable design needs a
real master-side child-input path; one bounded option is a second
application-owned PTY bridged to qtermwidget's remote-teletype slave, with
explicit child-winsize propagation and one owner per descriptor. Because
Accepted ADR-0030 commits to the invalid slave-forwarding design at
`docs/wiki/adr/0030-confine-qtermwidget-behind-terminal-adapter.md:39-52,78-80`,
the durable ownership correction must follow the project's superseding-ADR
policy rather than silently rewriting the decision.

## P1-2 — Failed escalation discards the survivor and still quits

`TerminalSession::completeShutdown(false, ...)` stops polling, destroys the
backend, and zeroes `m_childPid` before entering `ShutdownFailed`
(`src/apps/terminal/session/terminal_session.cpp:184-197`). The window prints
the error but emits `closeShutdownFinished()` even for `clean == false`
(`src/apps/terminal/ui/terminal_window.cpp:320-331`), and production queues that
signal to application quit. The SIGKILL survivor is therefore untracked and
orphaned, contradicting `docs/wiki/apps/terminal.md:51-68`.

The state is also replaceable despite the prose: `restart()` rejects only
`ShuttingDown` (`terminal_session.cpp:109-121`), the Restart action is enabled
for `ShutdownFailed` (`terminal_window.cpp:246-287`), and the already-zero pid
makes that restart report a clean teardown then spawn a new generation. The
test at `tests/apps/terminal/tst_terminal_session.cpp:325-346` asserts only
`start()` refusal and misses restart, retained ownership, and close/quit truth.

## P1-3 — Closing during Restart launches a fresh child before quit

Restart records `m_restartAfterShutdown=true`; close while the state is already
`ShuttingDown` changes only window intent, because `beginShutdown()` returns
without cancelling the pending restart (`terminal_session.cpp:109-127`;
`terminal_window.cpp:344-351`). `completeShutdown()` then emits
`shutdownFinished` (which queues application quit) and synchronously calls
`spawnGeneration()` before that queued quit executes
(`terminal_session.cpp:184-197`). An ordinary Restart→Close interaction thus
starts generation 2 and immediately destroys the process. Add an exact
close-cancels-restart regression proving no second backend/child is created.

## P1-4 — Select All creates an out-of-bounds real-widget selection

`TerminalWidgetAdapter::selectAllInView()` passes
`historyLinesCount()+screenLinesCount()` as the end row and the column count as
the end column (`terminal_widget_adapter.cpp:329-339`). These are counts, while
qtermwidget uses zero-based rows. Upstream corrects `x == columns` but never
clamps the one-past row; selected-text extraction then indexes
`screenLines[screenLines.count()]`:
[qtermwidget.cpp](https://github.com/lxqt/qtermwidget/blob/4ca192e/lib/qtermwidget.cpp#L675-L698),
[Screen.cpp](https://github.com/lxqt/qtermwidget/blob/4ca192e/lib/Screen.cpp#L1226-L1245),
[Screen.cpp](https://github.com/lxqt/qtermwidget/blob/4ca192e/lib/Screen.cpp#L1300-L1326),
[Screen.cpp](https://github.com/lxqt/qtermwidget/blob/4ca192e/lib/Screen.cpp#L1390-L1412).
Debug can assert and Release can access past the container when selection is
extracted/copied. The current fake only counts a routed call. Use the last
valid row, preserve upstream's explicit one-past-column convention, publish
selection availability, and exercise extraction through the real adapter.

## P2 findings

1. **Locale precedence still follows envp order.** At
   `terminal_launch_policy.cpp:207-224`, authority freezes on the first
   locale-named entry encountered, not the highest present variable in
   `LC_ALL > LC_CTYPE > LANG` order. `{LANG=en_US.UTF-8, LC_ALL=C}` therefore
   returns effective non-UTF-8 `LC_ALL=C`. Tests at
   `tests/apps/terminal/tst_launch_policy.cpp:231-314` always put the governing
   variable first. They also duplicate production's permissive “contains
   UTF8” oracle (`terminal_launch_policy.cpp:55-59` versus test lines 47-50),
   so a hostile invalid value containing that substring passes both.
2. **The post-fork child path allocates despite its contract.** `execChild()`
   claims to remain allocation-free at `terminal_widget_adapter.cpp:66-68`,
   but creates/reserves/pushes two `std::vector<char *>` arrays after `fork()`
   at lines 93-106. In a potentially multithreaded Qt process this is not an
   async-signal-safe pre-exec path. Prepare argv/envp pointers before fork (or
   use an equivalent safe spawn path) and check the dup/setup failures rather
   than executing with partial stdio state.
3. **The supposedly private dependency is PUBLIC.** The adapter's CMake links
   `qtermwidget6` as a `PUBLIC` usage requirement
   (`src/apps/terminal/CMakeLists.txt:48-62`), exporting its include/link usage
   to every consumer despite the “invisible to every other module” contract in
   `docs/wiki/architecture/module-boundaries.md:136-139` and
   `docs/wiki/apps/terminal.md:70-79`. Keep support public but make qtermwidget
   private/link-only to the static adapter.
4. **Action enabled state is stale and contradicts accessibility prose.** Only
   Copy is initially disabled (`terminal_window.cpp:186-201`), while
   `updateStatusForState()` toggles only Restart (`246-287`). Paste actions do
   not deactivate when no generation is live as claimed at
   `docs/wiki/apps/terminal.md:96-102`; selection availability is not cleared
   on view disposal, so Copy can stay enabled across restart and the direct
   Select All path never publishes availability. Add state/selection truth and
   fake-backed transitions independent of the real selection OOB repair.
5. **ECHILD is fabricated as a successful exit.** The production monitor maps
   `waitpid(...)=ECHILD` to `Exited, signaled=false, code=0`
   (`src/apps/terminal/session/process_liveness.cpp:24-53`), and the session
   publishes that as normal exit 0 (`terminal_session.cpp:241-251`). If another
   component or inherited SIGCHLD policy consumed status, the code is unknown;
   inventing success violates the advertised application-owned exit truth.
   Surface an invariant/error outcome rather than a false normal status.

## P3 notes

1. `resolveShell()` documents an executable regular-file/bounded hostile-path
   contract but checks directory/executable without `QFileInfo::isFile()` and
   has no explicit program/working-directory diagnostic bound
   (`terminal_launch_policy.cpp:82-180`; wiki `17-25`).
2. The child descriptor sweep stops at fd 4095 while its comment says every
   inherited descriptor closes, and the inherited signal mask is not reset
   (`terminal_widget_adapter.cpp:30-33,56-111`). Use a Linux-wide close primitive
   or a precomputed safe bound and deliberately reset the mask.
3. The predictable shared scheme file is opened truncate-in-place rather than
   atomically/per-instance (`terminal_widget_adapter.cpp:172-191`); concurrent
   launches can race its contents, and a pre-existing symlink can redirect the
   truncation. The existing deferral at `terminal.md:163-165` understates that
   failure mode.
4. Current public now contains QindaQt.AppShell, so the future-facing
   `main.cpp:26-29` AGENT-NOTE becomes stale on merge. Public `50742f` still has
   no ADR-0030 collision, but read-only `merge-tree` finds three textual
   conflicts (ADR index, wiki index, MkDocs) and three semantic auto-unions
   (module boundaries, source/test CMake). The manager must retain public
   ADR-0026/0027, AppShell, PB-0, Virtual Desktop, and candidate ADR-0030/
   Terminal entries; any additional conflict requires a fresh preflight.

## What passes at this source stopping point

- Normal close disables quit-on-last-window-closed before show and queues quit
  after clean fake-backed shutdown (`main.cpp:73-84,169-179`;
  `terminal_window.cpp:122-136`; test `317-367`).
- The pure fake-backed session model exercises close grace → TERM → KILL,
  typed code/signal publication, generation replacement, and forced
  Running/ShuttingDown destructor escalation. Those proofs do not cover the
  P1 close/restart/failure combinations above.
- Forced `TERM`/`COLORTERM` removes every inherited exact prefix and appends one
  policy value (`terminal_launch_policy.cpp:61-70,234-267`).
- The source requests qtermwidget fail-closed at `2.4...<2.5`, ADR-0030 is
  Accepted and all candidate links/number references consistently use 0030;
  no stale 0028/0029 Terminal reference remains.
- QST appearance derives all five themes and sixteen ANSI slots without app
  palette literals; stable Shift-modified actions, desktop identity/metadata,
  installed-theme probe shape, and source dependency direction are otherwise
  coherent.
- The registry contains exactly **seven** `^qindaqt\.terminal-` rows. Production
  files remain below the 500-nonblank decomposition threshold (maximum 363);
  tests are behavior-separated and the largest cohesive test is 477 nonblank
  lines. `git diff --check` passes on the exact tree.

Micah's reported scratch harness compiled only the qtermwidget-free support
library and ran four ad-hoc test executables (45 QtTest functions). It was
useful and correctly labelled, but it is neither the candidate's seven
registered CTest rows nor a build/live proof of the production adapter; it
cannot detect P1-1 or P1-4. No executable evidence may be credited to this
rereview.

## Required next action

Do not integrate `2386e74`. Route these exact reproductions to Micah (or a
strong repair partner for the PTY ownership change), preserve the candidate,
land a non-amended descendant with the required superseding ADR/wiki/test
updates, and return that exact commit to Church for rereview. Only after a
source PASS should the manager provision qtermwidget 2.4.x, run the registered
seven rows and combined-tree regressions, then execute one private real-adapter
PTY round trip proving keyboard→child, child→rendering, resize, select/copy,
exit status, close, close-during-restart, failed-escalation retention, first
frame, and PSS. This reviewer remains available.
