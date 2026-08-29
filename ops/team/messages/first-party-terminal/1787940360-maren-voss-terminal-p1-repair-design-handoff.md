# Maren Voss — Terminal P1 repair design handoff (implementation-ready, no code landed)

- Time: 2026-08-28T18:06:00Z
- Worker: Maren Voss, permanent Terminal S0 repair architecture analyst,
  Anthropic Claude Code `claude-fable-5`, reasoning high — posted by the
  live process
- Addressees: Tomas Reed (or the next Terminal implementer); Dijkstra the
  2nd (same-reviewer rereview); Katherine Cho; Program Manager
- Exact subject: `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b`
  (tree `87ed4cec98b1d8faf1a170514c29917286da108d`, parent `2386e74`);
  every file/line below is against that commit
- Prior evidence: Dijkstra `1787937173` (FAIL `0/3/3/4`), Church
  `1787931308`, Astra `20260828T110500`, my findings `1787940159`
- Status of the defects: **not fixed**. This is a design; nothing has been
  applied to any tree.

## 0. One-paragraph summary

All three P1s are small, disjoint, and repairable in one non-amended
descendant of `9bd5444`: (A) delete the five-line ShuttingDown special
case in `TerminalWindow::closeEvent` so a close during a pending Restart
reaches `TerminalSession::beginShutdown()`, which already cancels the
restart; (B) make `TerminalPtyBridge::pumpMasterToSink()` disable its read
notifier on any terminal read condition (`EOF`, `EIO`, hard error) while
keeping the master open; (C) move `TerminalWidgetAdapter::terminalWidget()`
out of line into the `.cpp` **and** fix four masked `-Werror` errors in the
pre-fork pointer-array code. Two new registered test functions (one window
row, one bridge row) plus the strict production build are the regression
gates. Roughly 40 changed product lines, no public boundary widens, no
ownership moves.

## 1. Failure A — Restart, then a real window Close, spawns generation 2

### A.1 Exact state transitions (candidate)

| Step | Where | State / flags after |
| --- | --- | --- |
| 1. User triggers `sessionRestartAction` | `terminal_window.cpp:182-187` → `TerminalSession::restart()` `terminal_session.cpp:111-126` → `enterShutdownSequence(true)` `:187-202` | `m_restartAfterShutdown=true`, `m_phase=Close`, `State::ShuttingDown`, view disposed, `backend->requestShutdown()` (bridge master closed → SIGHUP), poll timer running |
| 2. User closes the window (X, `fileQuitAction`, or WM close) | `TerminalWindow::closeEvent` `terminal_window.cpp:383-388`: `state()==ShuttingDown` branch | `m_quitRequested=true`, event accepted (Qt hides the window). **`beginShutdown()` is never called**, so `m_restartAfterShutdown` stays `true` |
| 3. Poll tick reaps generation 1 | `pollTick` `:292-303` → `completeShutdown(true, {})` `:204-226` | `m_backend.reset()`, `m_childPid=0`, `State::ShutdownComplete`, then line 217 `clean && m_restartAfterShutdown` is **true** → `spawnGeneration()` `:144-177`: new backend, `fork`/`execve` of a fresh shell, `State::Running`, `terminalWidgetChanged` (the window embeds it while hidden) |
| 4. Same call, line 225 | `emit shutdownFinished(true)` → `reportShutdownOutcome(true)` `terminal_window.cpp:352-370` | `m_quitRequested` → `emit closeShutdownFinished()` → queued `QCoreApplication::quit` (`main.cpp:178`) |
| 5. Event loop processes the queued quit | `main.cpp:180` returns; `window` (`:170`) destroyed before `monitor` (`:162`) | `~TerminalSession` `terminal_session.cpp:19-42`: state `Running` → `requestShutdown()` + immediate `SIGTERM`,`SIGKILL` to the group **without waiting or reaping** |

Result: a second shell is forked and immediately killed on every ordinary
Restart→Close; if the child has not yet completed `setsid()` when step 5
runs, `leaderStillLeadsItsGroup` (`process_liveness.cpp:17-20`) refuses the
signal and only the bridge-master SIGHUP remains. Nondeterministic, and it
contradicts `docs/wiki/apps/terminal.md:73-75`.

### A.2 Ownership/lifetime contract involved

- `TerminalSession` is the single owner of the child lifecycle
  (`terminal_session.h:18-23`). The **only** public entry that expresses
  "close intent" is `beginShutdown()` (`terminal_session.h:57-60`), and its
  ShuttingDown branch (`terminal_session.cpp:128-135`) already implements
  cancellation. The session side is correct; the presentation side skips it.
- `TerminalWindow` owns presentation only (`terminal_window.h:17-23`); it must
  not reason about restart flags itself — it must route intent through the
  session's public boundary.

### A.3 Smallest repair (window only)

`src/apps/terminal/ui/terminal_window.cpp:383-408` — delete lines 384-388
(the `ShuttingDown` early return) so the ShuttingDown close falls through to
`requestCloseShutdown()` (`:372-381`), which already sets `m_quitRequested`,
hides, and calls `m_session->beginShutdown()`. Keep the `ShutdownFailed`
refusal branch (`:389-405`) exactly as is. Resulting shape:

```cpp
void TerminalWindow::closeEvent(QCloseEvent *event) {
  if (m_session->state() == TerminalSession::State::ShutdownFailed) {
    ... unchanged refusal ...
    event->ignore();
    return;
  }
  // AGENT-GUARD: every non-refused close — including one that arrives while a
  // Restart's teardown is already in flight — must reach
  // TerminalSession::beginShutdown(); in ShuttingDown that call is the
  // cancellation of the pending restart (P1-3). Short-circuiting here made
  // Restart→Close spawn generation 2 in front of the queued quit.
  requestCloseShutdown();
  event->accept();
}
```

Also extend the AGENT-GUARD at `:373-377` with one sentence saying the
method is idempotent while ShuttingDown. No session change is needed for A.

Why this is safe in every other state: `Idle`/`Exited`/`ShutdownComplete` →
`enterShutdownSequence(false)` with no child → `completeShutdown(true)` on
the next tick (existing behaviour); `Running` → unchanged normal close;
double close while ShuttingDown → second `beginShutdown()` is a flag-only
no-op, and `shutdownFinished` fires once, so `closeShutdownFinished` fires
once.

### A.4 Required regression (window row, `tests/apps/terminal/tst_terminal_window.cpp`)

1. Extend `WindowHarness` (`:102-125`) with `int createdBackends = 0;`
   incremented inside the factory lambda (capture `int *count =
   &createdBackends;`).
2. Add slot `restartThenCloseSpawnsNothingBeforeQuit()`:

```cpp
void TerminalWindowTest::restartThenCloseSpawnsNothingBeforeQuit() {
  // P1 (Dijkstra P1-2 / Church P1-1): the production close route during a
  // pending Restart must cancel the restart; generation 2 must never be
  // spawned in front of the queued application quit.
  WindowHarness harness; // InstantExitMonitor: generation 1 reaps on the first poll tick
  auto window = harness.makeWindow();
  QSignalSpy widgetSpy(window->session(), &TerminalSession::terminalWidgetChanged);
  QSignalSpy shutdownSpy(window->session(), &TerminalSession::shutdownFinished);
  QSignalSpy quitSpy(window.get(), &TerminalWindow::closeShutdownFinished);
  QVERIFY(window->session()->start(validRequest()));
  window->show();
  QVERIFY(QTest::qWaitForWindowExposed(window.get()));

  auto *restart = window->findChild<QAction *>(QStringLiteral("sessionRestartAction"));
  auto *quit = window->findChild<QAction *>(QStringLiteral("fileQuitAction"));
  QVERIFY(restart != nullptr && quit != nullptr);
  restart->trigger(); // real action route: enterShutdownSequence(true)
  QCOMPARE(window->session()->state(), TerminalSession::State::ShuttingDown);
  quit->trigger();    // real close route: closeEvent while ShuttingDown
  QVERIFY(!window->isVisible());
  window->close();    // double close while ShuttingDown must stay idempotent
  QTest::qWait(200);

  QCOMPARE(shutdownSpy.count(), 1);
  QVERIFY(shutdownSpy.first().first().toBool());
  QCOMPARE(window->session()->state(), TerminalSession::State::ShutdownComplete);
  QCOMPARE(harness.createdBackends, 1);     // no generation 2
  QCOMPARE(widgetSpy.count(), 1);           // one widget publication, ever
  QVERIFY(window->session()->terminalWidget() == nullptr);
  QCOMPARE(quitSpy.count(), 1);             // quit exactly once, after generation 1 is terminal
}
```

Determinism: the 1 ms poll timer cannot fire between `restart->trigger()`
and `quit->trigger()` because no event loop runs there; the reap happens
only inside `qWait`. This row **fails on the candidate** with
`createdBackends == 2` and `widgetSpy.count() == 2`.

3. Reword the comment at `tst_terminal_session.cpp:418` (`// The close
   path: cancels the pending restart.`) to say it is the session-level
   cancellation that `TerminalWindow::closeEvent` must route through, and
   name the new window row. Keep that session test.

### A.5 Invariants to state in code/docs

- **I-A1** `closeShutdownFinished` (hence quit) is emitted only from
  `reportShutdownOutcome(clean=true)`, i.e. after `completeShutdown` has
  released the backend and `m_childPid == 0`, and no generation is spawned
  between a close request and that emission.
- **I-A2** Every non-refused `closeEvent` reaches
  `TerminalSession::beginShutdown()`; presentation never inspects or clears
  restart intent itself.

## 2. Failure B — retained Exited generation spins on PTY `EIO`/`POLLHUP`

### B.1 Exact transitions (candidate)

| Step | Where | Effect |
| --- | --- | --- |
| 1. Bridge opened in adapter ctor | `pty_bridge.cpp:26-81`; read notifier created and enabled `:68-71` | Master `O_NONBLOCK`; no slave open yet → `poll` idle, `read`=`EAGAIN` (measured) — no spin before start |
| 2. Child opens the slave by path, runs, exits (or the last slave holder closes) | kernel | Master becomes `POLLIN\|POLLHUP`; buffered output still readable |
| 3. Notifier fires | `pumpMasterToSink()` `:155-179` | Drains data to the sink; then `read` = `-1/EIO` → line 177 `return` **with the notifier still enabled** |
| 4. Every subsequent event-loop iteration | `QEventDispatcherGlib` polls `G_IO_IN\|G_IO_HUP` | `POLLHUP` persists → notifier re-activates → `EIO` again. Measured: **75,566 activations in 200 ms** |
| 5. Session reaps (≤20 ms later) | `pollTick` `terminal_session.cpp:270-289` | `State::Exited`, poll timer stopped, **backend deliberately retained** for scrollback → bridge + master + hot notifier live until Restart/Close |

### B.2 Ownership/lifetime contract involved

- The bridge master has exactly one closer: `closeChildChannel()`
  (`pty_bridge.cpp:103-116`), which is the teardown SIGHUP path invoked from
  `TerminalWidgetAdapter::requestShutdown()`/dtor. `isOpen()` (`pty_bridge.h:57`)
  gates `start()` and resize. Therefore the read path must **not** close the
  master; it may only stop listening.
- Exit truth is `ProcessMonitor::reap` only (`terminal_session_backend.h:11-18`,
  adapter `:193-195`). `EIO` is not an exit signal: it occurs while a child
  is alive if it closed its stdio, and does not occur when a dead child left
  a grandchild holding the slave.

### B.3 Smallest repair (bridge only)

`src/apps/terminal/session/pty_bridge.h`: add `bool m_childOutputClosed =
false;` (`:64-69` member block) and the accessor
`[[nodiscard]] bool isChildOutputClosed() const { return m_childOutputClosed; }`
next to `isOpen()` (`:57`). Extend the class contract comment (`:13-27`)
with: "once the read side reports a terminal condition (EOF, `EIO` after the
last slave closes, or a hard error) the read notifier is disabled for the
rest of the generation; the master stays open until `closeChildChannel()`,
which remains the only close and the escalation's SIGHUP path."

`src/apps/terminal/session/pty_bridge.cpp:155-179` becomes:

```cpp
void TerminalPtyBridge::pumpMasterToSink() {
  if (m_masterFd < 0 || m_childOutputClosed) {
    return;
  }
  char chunk[8192];
  for (;;) {
    const ssize_t received = ::read(m_masterFd, chunk, sizeof(chunk));
    if (received > 0) {
      if (m_sink) { m_sink(chunk, static_cast<int>(received)); }
      if (received < static_cast<ssize_t>(sizeof(chunk))) { return; }
      continue;
    }
    if (received < 0 && errno == EINTR) { continue; }
    if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return; // Drained; the notifier stays armed.
    }
    // AGENT-GUARD: 0 (EOF) or any other errno — Linux reports EIO once the
    // last slave descriptor is gone and keeps the master POLLHUP-readable
    // forever — is terminal for this generation. Leaving the notifier
    // enabled here hot-loops the GUI thread (P1-3). The master is NOT
    // closed: closeChildChannel() is its only owner and the SIGHUP path,
    // and exit truth stays with the session's ProcessMonitor reap.
    m_childOutputClosed = true;
    if (m_readNotifier != nullptr) {
      m_readNotifier->setEnabled(false);
    }
    return;
  }
}
```

Also reset `m_childOutputClosed = false;` at the top of `open()` for
hygiene (the adapter never reopens a bridge, but the contract should not
depend on that). `flushInput()` (`:127-153`) needs no change: on a hung-up
master its error branch already clears the buffer and the write notifier
ends disabled at `:150-152`.

### B.4 Required regression (bridge row, `tests/apps/terminal/tst_pty_bridge.cpp`)

Add slot `slaveCloseQuiescesReadNotifierAndKeepsMaster()`:

```cpp
void TerminalPtyBridgeTest::slaveCloseQuiescesReadNotifierAndKeepsMaster() {
  // P1 (Dijkstra P1-3): after the last slave closes, the master stays
  // POLLHUP-readable and read() reports EIO forever. The bridge must drain
  // what was buffered, then disable its read notifier while retaining the
  // master (closeChildChannel() is the only close and the SIGHUP path).
  std::vector<QByteArray> sink;
  TerminalPtyBridge bridge(
      [&sink](const char *data, int length) { sink.emplace_back(data, length); }, this);
  const auto opened = bridge.open();
  QVERIFY(opened.ok);
  QSocketNotifier *readNotifier = nullptr;
  for (auto *notifier : bridge.findChildren<QSocketNotifier *>()) {
    if (notifier->type() == QSocketNotifier::Read) { readNotifier = notifier; }
  }
  QVERIFY(readNotifier != nullptr);
  QVERIFY(readNotifier->isEnabled());
  QVERIFY(!bridge.isChildOutputClosed());

  const int slave = openRawSlave(opened.slavePath);
  QVERIFY(slave >= 0);
  const QByteArray last = QByteArrayLiteral("bye");
  QCOMPARE(::write(slave, last.constData(), static_cast<size_t>(last.size())),
           static_cast<ssize_t>(last.size()));
  ::close(slave); // last slave descriptor: master is now hung up

  // Buffered output drains before the terminal condition is honoured.
  QVERIFY(QTest::qWaitFor([&sink, last] {
    QByteArray all; for (const auto &chunk : sink) { all += chunk; }
    return all.contains(last); }, kSinkWaitMs));
  QTRY_VERIFY_WITH_TIMEOUT(bridge.isChildOutputClosed(), kSinkWaitMs);
  QVERIFY(!readNotifier->isEnabled());
  QVERIFY(bridge.isOpen()); // master retained: single-owner close

  // Bounded liveness: a hot notifier activates thousands of times here.
  int activations = 0;
  connect(readNotifier, &QSocketNotifier::activated, this,
          [&activations] { ++activations; });
  const auto sinkCallsBefore = sink.size();
  QTest::qWait(100);
  QCOMPARE(activations, 0);
  QCOMPARE(sink.size(), sinkCallsBefore);

  bridge.writeInput("x", 1); // input after hangup must not re-arm reading
  QTest::qWait(20);
  QCOMPARE(activations, 0);
  bridge.closeChildChannel();
  QVERIFY(!bridge.isOpen());
}
```

This row **fails on the candidate** at `QTRY_VERIFY(isChildOutputClosed())`
(accessor absent → compile) or, with the accessor but without the
`pumpMasterToSink` change, at `QCOMPARE(activations, 0)` with a count in the
tens of thousands. `openRawSlave` (`:27-38`) keeps the transport raw so the
3 bytes arrive unchanged. `findChildren` is white-box but honest: both
notifiers are parented to the bridge (`pty_bridge.cpp:68-77`).

### B.5 Invariants

- **I-B1** The bridge master is closed only by `closeChildChannel()`; the
  read path never closes it. `isOpen()` means "master owned", not "child
  alive".
- **I-B2** After a terminal read condition the read notifier stays disabled
  for the generation; `isChildOutputClosed()` is an observation for tests
  and diagnostics, never an exit signal — exit truth is `reap()` only.
- **I-B3** Classification is exact: `>0` data, `EINTR` retry,
  `EAGAIN/EWOULDBLOCK` drained-and-armed, everything else terminal. Any
  looser rule either spins (too permissive) or silences a live child (too
  strict).

## 3. Failure C — strict production adapter does not compile

### C.1 Exact cause

`src/apps/terminal/ui/terminal_widget_adapter.h:12` forward-declares
`class QTermWidget;` and `:46` defines
`QWidget *terminalWidget() override { return m_widget; }` inline. A
derived-to-base pointer conversion requires the complete derived type.
Three TUs include the header without a complete `QTermWidget`: the
AUTOMOC unit, `main.cpp:6`, **and the adapter `.cpp` itself** (`:2` includes
the header before `:7` includes `<qtermwidget.h>`). All three fail with
`cannot convert 'QTermWidget*' to 'QWidget*'` (reproduced with Dijkstra's
exact flags; see `1787940159`).

Behind that first error, the same TU has four more `-Werror` failures in
the P2-2 pre-fork pointer-array code from the previous repair:
`:291`/`:295` `reserve(qsizetype)` sign-conversion; `:302`/`:306`
`const char *` from `const QByteArray &::data()` pushed into
`std::vector<char *>`.

### C.2 Boundary contract involved

- `qtermwidget.h` is included by exactly one TU
  (`terminal_widget_adapter.cpp:7`; ADR-0040; `terminal_widget_adapter.h:18-20`).
- `qtermwidget6` is a **PRIVATE** link dependency of the static adapter
  (`src/apps/terminal/CMakeLists.txt:51-71`, P2-3): consumers such as
  `main.cpp` compile **without** the qtermwidget include path. The header
  must therefore stay compilable with a forward declaration only.
- `TerminalSessionBackend::terminalWidget()` returns `QWidget *`
  (`terminal_session_backend.h:50`); the signature must not change.

### C.3 Smallest repair (five hunks, all inside the adapter's own files)

1. `terminal_widget_adapter.h:46` →
   `[[nodiscard]] QWidget *terminalWidget() override;` with a two-line
   comment: defined out of line because `QTermWidget` is only
   forward-declared here.
2. `terminal_widget_adapter.cpp`, after the constructor (before `:200`
   `~TerminalWidgetAdapter`): `QWidget *TerminalWidgetAdapter::terminalWidget()
   { return m_widget; }` — `QTermWidget` is complete there.
3. `:291` → `strings.arguments.reserve(static_cast<size_t>(request.arguments.size()));`
4. `:295` → `strings.environment.reserve(static_cast<size_t>(request.environment.size()));`
5. `:301` and `:305` → iterate `QByteArray &` (non-const) so `data()` yields
   `char *` for `argv`/`envp`. Add an AGENT-GUARD above `:300`: the pointer
   arrays alias the `QByteArray` buffers, each of which has refcount 1 after
   construction; never append to `arguments`/`environment` after the
   pointer arrays are built, and never copy `ExecStrings` (a copy would
   share buffers and a later non-const `data()` would detach and dangle).
   `execChildInBridge` takes `const ExecStrings &`, so no copy exists today.

Validated on a private copy with the exact strict flags: adapter TU exit 0,
moc unit exit 0, `main.cpp` exit 0, link against the exact-candidate support
/themes/tokens archives and pinned `libqtermwidget6.so.2` exit 0, CLI
positional rejection exit 2, `--check-theme` exit 0.

### C.4 Required regression

There is no unit test for a compile error; the gate is the strict
production build of `qindaqt_terminal_adapter` and `qindaqt-terminal`
becoming mandatory in the acceptance sequence (§7 step 3) and the two
executable-dependent registered rows (`qindaqt.terminal-cli-rejects-positional-arguments`,
`qindaqt.terminal-installed-metadata`) passing, which they cannot today.
Optional but cheap: keep the header self-sufficient by convention — any
future inline body that touches `m_widget` reintroduces C.1; state that in
the header comment.

### C.5 Invariants

- **I-C1** `terminal_widget_adapter.h` compiles in a TU that has only Qt and
  the support headers; `QTermWidget` stays forward-declared there.
- **I-C2** The adapter `.cpp` compiles with the repository's strict flags;
  the scratch support-only harness cannot stand in for it.

## 4. Interactions between the three seams (and with the open P2/P3s)

- **A ↔ B.** Disjoint code, but they meet at teardown: `enterShutdownSequence`
  → `requestShutdown()` → `closeChildChannel()` deletes the notifiers and
  closes the master (`pty_bridge.cpp:103-116`). If B already disabled the
  notifier, deletion is still correct. Conversely B never changes state that
  A reads. Close from `Exited` (child already reaped) → second `waitpid`
  returns `ECHILD` → `Exited, statusKnown=false` → `completeShutdown(true)`:
  existing behaviour, unchanged by either fix.
- **B ↔ P2-1 (paste enabled in Exited).** B does not gate input; after the
  read side is closed `writeInput` still writes into a hung-up master
  (harmless, buffer cleared on error). The user-facing gate is the window's
  `updateViewActionStates` (`terminal_window.cpp:251-266`) enabling paste on
  `Running` only — a different function from A's `closeEvent`, so the two
  window edits do not collide textually, but both new window rows need the
  `WindowHarness` change; make it once.
- **B ↔ P2-3 (double line discipline).** P2-3 edits the widget-slave termios
  in the adapter constructor (`terminal_widget_adapter.cpp:150-168`); B edits
  the bridge read loop. Disjoint. Land B first so the P2-3 change is
  exercised against a quiescent bridge.
- **C ↔ P2-2 (headless rows).** C makes the executable buildable, which is
  what lets the two executable-dependent rows run at all. Until P2-2 lands,
  the acceptance command must export `QT_QPA_PLATFORM=offscreen` globally
  (four rows abort otherwise, Dijkstra 1/5) — state that in the handoff, do
  not silently rely on a desktop session.
- **C ↔ P3-2 / P3-4.** Same `.cpp`: P3-2 is `closeChildDescriptors` (`:77-90`),
  P3-4 is the `open`/`setsid` order in `execChildInBridge` (`:99-113`); C is
  `:46`, `:200`, `:291-306`. Separate hunks, no ordering constraint, but a
  reviewer diffing the adapter will see all of them — keep each hunk's
  comment naming its finding.
- **A ↔ P1-2 (already fixed) ShutdownFailed.** A's fall-through must keep the
  `ShutdownFailed` refusal branch first; Restart→Close→escalation failure
  then still yields `completeShutdown(false)` → `m_restartAfterShutdown=false`
  (`:223`), `reportShutdownOutcome(false)` re-shows the window, no quit.
  `quitIsRefusedWhileSurvivorRemains` (`tst_terminal_window.cpp:444-470`)
  continues to cover it.

## 5. Tempting but incorrect changes

- **A:** emitting `closeShutdownFinished` directly from `closeEvent` while
  ShuttingDown (quit before terminal state); reordering `completeShutdown`
  to emit `shutdownFinished` before `spawnGeneration()` (quit is queued, the
  spawn still happens synchronously — no fix); having the window clear a
  restart flag through a new session setter (duplicates `beginShutdown()`'s
  contract); making `beginShutdown()` in ShuttingDown re-arm the phase timer
  (extends the grace and can double-signal).
- **B:** closing the master on `EIO` (breaks the single-owner close, the
  SIGHUP path, `isOpen()` gating, and could let the pts number be reused
  under a still-tracked pid); fixing it in the session on `State::Exited`
  (misses the ≤20 ms pre-reap window and the grandchild case); treating
  `EIO` as an exit publication (violates the exit-truth contract in both
  directions); using `deleteLater()` on the notifier from inside its own
  slot instead of `setEnabled(false)` — works, but leaves a dangling
  `m_readNotifier` pointer for `closeChildChannel()`'s `delete`.
- **C:** including `<qtermwidget.h>` in the header (leaks the PRIVATE
  dependency; `main.cpp` has no qtermwidget include path and would fail,
  inviting a PUBLIC link that reverts P2-3); `reinterpret_cast<QWidget *>`
  in the header (not a derived-to-base conversion; wrong under multiple
  inheritance); changing `m_widget` to `QWidget *` with casts at every use;
  changing the backend interface's return type; `const_cast<char *>` on
  `constData()` instead of a non-const loop (works, but the non-const loop
  is the honest form given the buffers are owned and unshared).

## 6. Order of operations, commit shape, docs

1. Verify base: `git rev-parse HEAD` = `9bd54448…`, `git status --porcelain`
   empty, in the implementer's own worktree on `worker/terminal-s0` (or a
   branch at that commit). Never amend `9bd5444`.
2. **C first** (five hunks). Reason: it is the only hunk whose evidence needs
   the extracted package, and it unblocks the executable rows and any later
   live gate. Build `qindaqt_terminal_adapter` and `qindaqt-terminal`.
3. **B** (bridge `.h`/`.cpp` + new bridge row). Run the bridge executable.
4. **A** (window `closeEvent` + AGENT-GUARD + harness counter + new window
   row + session-test comment). Run the window executable.
5. Docs in the same commit: `docs/wiki/apps/terminal.md:73-75` add "including
   when the close arrives while the restart's teardown is already in
   flight"; `:78-94` add one sentence on read-side quiescence with master
   retention; `:149-165` add the two new rows to the covered list;
   `docs/wiki/adr/0040-…md:44-46` extend the buffer/EINTR bullet with the
   terminal-read rule. Registered row count stays 8; QtTest function counts
   become bridge 8, window 13, session 17.
6. Static gates: `git diff --check`, `python3 tools/check-source-shape`,
   `python3 tools/validate-docs`, `/tmp/opencode/mkdocs-venv/bin/mkdocs build
   --strict --site-dir <private>` (adapter stays well under 500 nonblank).
7. One non-amended descendant commit, e.g. "Repair Terminal Restart-Close
   cancellation, PTY read quiescence, and adapter strict compile", body
   naming the three P1s, the four masked errors, and each gate's exit/counts.
8. Post the handoff (exact SHA/tree/parent, manifest hash, commands, counts,
   caveats) and request Dijkstra's same-reviewer rereview. If the implementer
   also owns the P2/P3 findings, land them as the **next** descendant so the
   P1 subject stays small and independently reviewable; if the manager wants
   one commit, keep the P1 hunks first in the diff and unchanged in shape.

## 7. Acceptance commands (exact; all output under a private run directory)

```sh
WT=<implementer worktree>; RUN=<private run dir>; PFX=/tmp/dijkstra-terminal-gate.8l5adE/prefix/usr
# If that leftover prefix is gone, re-create it without a system install:
#   curl -O "$(pacman -Sp qtermwidget)" && mkdir -p prefix && tar -C prefix -xf qtermwidget-2.4.0-1-x86_64.pkg.tar.zst
git -C "$WT" rev-parse HEAD HEAD^ HEAD^{tree}; git -C "$WT" status --porcelain      # HEAD^ = 9bd54448…, empty status
cmake -S "$WT" -B "$RUN/build" -G Ninja -DBUILD_TESTING=ON -DQINDAQT_BUILD_SHELL=OFF \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=OFF -DQINDAQT_BUILD_KWIN_PLUGIN=OFF \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$PFX"                              # exit 0
cmake --build "$RUN/build" --parallel 1 --target qindaqt_terminal_support \
  qindaqt_terminal_adapter qindaqt-terminal qindaqt_terminal_launch_policy_tests \
  qindaqt_terminal_pty_bridge_tests qindaqt_terminal_session_tests \
  qindaqt_terminal_appearance_tests qindaqt_terminal_window_tests               # exit 0 (C gate)
QT_QPA_PLATFORM=offscreen LD_LIBRARY_PATH="$PFX/lib" \
  ctest --test-dir "$RUN/build" -R '^qindaqt\.terminal-' --output-on-failure      # 8/8, exit 0
QT_QPA_PLATFORM=offscreen "$RUN/build/tests/apps/terminal/qindaqt_terminal_pty_bridge_tests"   # 8 passed (B gate)
QT_QPA_PLATFORM=offscreen "$RUN/build/tests/apps/terminal/qindaqt_terminal_window_tests"       # 13 passed (A gate)
QT_QPA_PLATFORM=offscreen "$RUN/build/tests/apps/terminal/qindaqt_terminal_session_tests"      # 17 passed (unchanged)
git -C "$WT" diff --check; (cd "$WT" && python3 tools/check-source-shape && python3 tools/validate-docs)
/tmp/opencode/mkdocs-venv/bin/mkdocs build --strict -f "$WT/mkdocs.yml" --site-dir "$RUN/site"
```

Negative controls worth running once to prove the rows are not vacuous:
temporarily revert only hunk A → window row fails with `createdBackends==2`;
revert only the `pumpMasterToSink` hunk → bridge row fails at
`QCOMPARE(activations, 0)`. Do not commit either revert.

`QT_QPA_PLATFORM=offscreen` on the ctest line is required **because P2-2 is
still open**; the installed-metadata row sets offscreen itself
(`run_installed_terminal.cmake:35`) and inherits `LD_LIBRARY_PATH` for the
staged binary. Anything beyond these commands (real shell under the real
adapter, keyboard→child, resize, select/copy, signal exits, first frame,
PSS) remains the serialized private live gate and is not claimed here.

## 8. Failure rollback

Each fix is one revertible hunk group with a distinct failure signature:

- A fails its row → `createdBackends`/`widgetSpy` = 2: the close still
  short-circuits before `beginShutdown()`. Revert only `closeEvent`.
- B fails its row → `activations` ≫ 0: an errno was misclassified as
  drained; `isChildOutputClosed()` never set → the terminal branch is not
  reached. Revert only `pumpMasterToSink` (+ header flag).
- C fails the strict build → a new inline use of `m_widget` in the header or
  a remaining sign/const error; the compiler line number names it. Revert
  only the offending hunk. Reverting C reverts nothing of A or B, and vice
  versa: no hunk depends on another to compile or pass.

## 9. No-code / no-mutation statement

I wrote no product code into any tree. The candidate worktree
`/home/cabewse/work_SPaC3/container-wm-workers/terminal-s0-analysis-maren`
was never edited, staged, formatted, or committed; its tracked tree is
clean at `9bd54448`. The only executed evidence is the private
single-translation-unit compile/link and the Qt Core PTY probe recorded in
`1787940159`, both confined to
`/home/cabewse/work_SPaC3/container-wm-private-agent-runs/maren-terminal-analysis/`
(`repro-p1c/`, `repro-p1b/`, with `result*.log`). No tree configure, no
CTest, no GUI, no PTY child, no host session/input. The three defects are
**not fixed**; this handoff is the design for the implementer's non-amended
descendant and Dijkstra's rereview.
