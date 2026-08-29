# Church the 2nd — Terminal S0 P1: close during restart launches a fresh child before quitting

- Time: 2026-08-28T08:46:33-06:00
- Owner: Church the 2nd
- Addressees: Micah Stone; Program Manager
- Exact candidate: `2386e7464bcebe17dd074299ac20f1739a5bf8b1`
- Severity: P1; close must cancel an in-flight restart

There is a deterministic normal-user race in the repaired teardown-first quit
flow. `TerminalSession::restart()` sets `m_restartAfterShutdown = true` through
`enterShutdownSequence(true)` (`src/apps/terminal/session/terminal_session.cpp:109-121,170-182`).
If the user closes the window during that bounded teardown, `closeEvent()` sets
only the window's `m_quitRequested` because the session is already
`ShuttingDown` (`src/apps/terminal/ui/terminal_window.cpp:344-351`), while
`TerminalSession::beginShutdown()` returns without clearing the pending restart
(`terminal_session.cpp:123-127`).

On clean teardown, `completeShutdown()` first emits `shutdownFinished`; the
window therefore emits `closeShutdownFinished`, which production queues to
`QCoreApplication::quit`. The same `completeShutdown()` call then observes the
still-true restart flag and synchronously calls `spawnGeneration()` before the
queued quit runs (`terminal_session.cpp:184-197`). The app thus launches a new
PTY child and immediately exits; only the destructor's unbounded/immediate
best-effort TERM→KILL guard remains. This defeats the documented close
guarantee under an ordinary Restart-then-Close interaction.

Repair should make an explicit close shutdown cancel the pending restart (for
example, `beginShutdown(false)` updates the intent even while already
ShuttingDown), and a combined regression must prove Restart → Close never
creates generation 2 and emits quit only after generation 1 is terminal. This
is separate from the SIGKILL-survivor ownership failure in `1787928736`.

No product/Git mutation, compiler, PTY, GUI, session, or host interaction was
performed. Full exact verdict is still in progress.
