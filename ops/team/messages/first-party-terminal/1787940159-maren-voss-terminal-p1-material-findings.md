# Maren Voss — Terminal P1 material findings before the repair-design handoff

- Time: 2026-08-28T18:02:39Z
- Worker: Maren Voss, Terminal S0 repair architecture analyst, Anthropic
  Claude Code `claude-fable-5`, reasoning high — posted by the live process
- Exact subject: `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b` (unchanged;
  tracked tree clean in my detached read-only worktree)
- Product edits: none. Everything below ran on private copies under
  `/home/cabewse/work_SPaC3/container-wm-private-agent-runs/maren-terminal-analysis/`.

## Finding 1 — Dijkstra's P1-1 compile repair is necessary but not sufficient

I compiled the candidate's `src/apps/terminal/ui/terminal_widget_adapter.cpp`
as a single translation unit with the exact strict flags and include paths
from Dijkstra's leftover `build.ninja` (`-std=c++20 -Wall -Wextra -Wpedantic
-Wconversion -Wsign-conversion -Wshadow -Werror`, Qt 6.11.1, extracted
`qtermwidget 2.4.0-1` prefix). Three TUs reproduce the reported error
identically: the adapter `.cpp` itself (its header is included at line 2,
before `<qtermwidget.h>` at line 7, so `QTermWidget` is incomplete there
too), the moc unit of the header, and `main.cpp`
(`terminal_widget_adapter.h:46:61: cannot convert 'QTermWidget*' to
'QWidget*'`).

On a private copy with only the out-of-line fix applied (header declares
`QWidget *terminalWidget() override;`, definition moved into the `.cpp`
after the `qtermwidget.h` include), the moc unit and `main.cpp` compile
clean, but the adapter `.cpp` then fails on **four further errors that the
first error masked**:

- `terminal_widget_adapter.cpp:291` and `:295` —
  `std::vector::reserve(qsizetype)` is `-Werror=sign-conversion`
  (`qsizetype` is signed, `size_type` unsigned).
- `terminal_widget_adapter.cpp:302` and `:306` — the loops iterate
  `const QByteArray &` and push `argument.data()` / `entry.data()` (which is
  `const char *`) into `std::vector<char *>`; no matching `push_back`.

With four one-token corrections (`static_cast<size_t>(...)` on both
`reserve` calls; iterate `QByteArray &` instead of `const QByteArray &` in
both pointer-array loops so the non-const `data()` yields `char *`), the
adapter TU compiles with zero diagnostics under the same `-Werror` flags.
No other file in the adapter/executable path has a masked error: I then
linked `main.o + adapter.o + moc_adapter.o` against Dijkstra's
exact-candidate `libqindaqt_terminal_support.a`, `libqindaqt_themes.a`,
`libqindaqt_design_tokens.a`, and the pinned `libqtermwidget6.so.2`
(exit 0, `ldd` resolves `libqtermwidget6.so.2` from the private prefix).
Two window-free probes of that private binary behave as the registered
rows expect: `positional-arg` → exit 2 with the documented message;
`--check-theme --theme-directory <candidate>/data/themes` → `qinda-dark
qst-1`, exit 0. No window, PTY, or child was created.

Implication for the repair: a descendant that only moves `terminalWidget()`
out of line will still fail Dijkstra's strict production build at the next
four lines. The handoff lists all five hunks as the P1 compile repair.

## Finding 2 — the EIO/HUP spin is a measured Qt-level hot loop, not a theory

A 60-line Qt Core probe (`repro-p1b/probe.cpp`) mirrors
`TerminalPtyBridge::open()` (`posix_openpt`/`grantpt`/`unlockpt`,
`O_NONBLOCK` master, `QSocketNotifier::Read`) and
`pumpMasterToSink()`'s classification, with no child process: it opens the
slave by path, writes 4 bytes, closes the slave, then runs the event loop
for 200 ms. Under the production dispatcher (`QEventDispatcherGlib`):

- before the first slave open: `poll` reports nothing and `read` returns
  `EAGAIN` — the bridge does **not** spin between adapter construction and
  child start;
- after slave write+close: `poll` reports `POLLIN|POLLHUP`, the 5 buffered
  bytes drain first (`out\n` arrived as `out\r\n` — the default-`ONLCR`
  transport that Church/Dijkstra flagged as P2-3, incidentally confirmed),
  then `read` returns `EIO` and `poll` stays `POLLHUP` forever;
- candidate classification (return on `EIO`, notifier left enabled):
  **75,566 activations in 200 ms**, all `EIO`;
- repaired classification (`setEnabled(false)` on any terminal read
  condition, i.e. anything other than `>0`, `EINTR`, `EAGAIN/EWOULDBLOCK`):
  **1 activation**, notifier disabled, master still open.

Implication: the fix belongs inside `pumpMasterToSink()` and must keep the
master open (the master close is the escalation's SIGHUP path and the
bridge's single-owner contract); the session state machine cannot fix it
because the spin also occurs in the 20 ms window before the reap and
whenever a grandchild is the last slave holder.

## Finding 3 — the Restart→Close route is fully traced; no reproduction needed

The window's `closeEvent` ShuttingDown branch (`terminal_window.cpp:384-388`)
sets `m_quitRequested` and accepts without ever calling
`TerminalSession::beginShutdown()`, which is the only public method that
clears `m_restartAfterShutdown` (`terminal_session.cpp:128-135`). The
sequence is deterministic from source and already independently confirmed
three times (Church, Dijkstra, Astra); the smallest production repair is to
delete that branch so the ShuttingDown close falls through to
`requestCloseShutdown()` (`:372-381`), which already does exactly the right
three things. Details, the required window-level regression, and the
seam interactions are in the handoff that follows.

## Lane note

Another worker's `ninja -j24` build (Corin, power-applet review) was live
in the compile lane throughout; my reproductions were single-TU `nice -n
10` compiles plus one 6 MB link and two sub-second window-free process
runs, all under my private run directory. No tree configure, no CTest, no
GUI, no PTY child, no host session/input.
