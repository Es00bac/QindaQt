# ADR-0040: Own the Terminal child PTY and bridge it through the teletype

- **Status:** Accepted
- **Date:** 2026-08-28
- **Owners:** Terminal application (`src/apps/terminal`)
- **Supersedes:** ADR-0030
- **Superseded by:** None

## Context

ADR-0030 accepted a teletype design in which the adapter wrote emulator
keyboard bytes back into qtermwidget's exposed **slave** descriptor. Exact
rereview (Church the 2nd, P1-1) established two fatal properties of that
design, confirmed against pinned qtermwidget 2.4.0 source:

- a slave write is terminal **output** toward the master (and therefore the
  emulator), never child input; keyboard and paste never reached the shell;
- applying `O_NONBLOCK` to a duplicated slave descriptor changes the shared
  open-file-description flags, so the child's `dup2`'d stdin/stdout/stderr
  silently became nonblocking.

qtermwidget exposes no master-side writer through its public API, so no
amount of buffering on the slave can repair the direction. The durability
rule for accepted decisions requires a superseding ADR rather than a silent
rewrite.

## Decision

The Terminal adapter owns a second, application-side PTY — the **bridge** —
implemented in the qtermwidget-free `session/pty_bridge` unit:

- the child's controlling TTY and stdio are the bridge PTY's **slave**, opened
  by path inside the child after `setsid()`/`TIOCSCTTY`. No bridge or widget
  descriptor is inherited, so child stdio keeps blocking semantics;
- keyboard and paste bytes are written to the bridge **master** — the only
  PTY direction that is child input;
- child output and line-discipline echo are read from the bridge master and
  forwarded to a private adapter-owned duplicate of the widget's teletype
  slave; slave write → widget master read → emulator is the correct output
  direction into the renderer, and the adapter clears output processing
  (`OPOST`, verified fail-closed) on that duplicate because the forwarded
  bytes are already line-disciplined — a transforming slave would mutate
  exact output a second time;
- child winsize is programmed explicitly (`TIOCSWINSZ` on the bridge master)
  from the live emulator grid when the widget resizes, so SIGWINCH reaches
  the child;
- each descriptor has exactly one writer, both directions use bounded
  (64 KiB, drop-newest) buffers with `EINTR` retry, and closing the bridge
  master is the teardown SIGHUP path;
- once the bridge read side reports a terminal condition (EOF, Linux `EIO`
  after the last slave descriptor closes, or a hard read error), its read
  notifier is disabled for the rest of the generation: Linux keeps a hung-up
  master `POLLHUP`-readable forever, so an armed notifier would spin the GUI
  thread while a retained Exited session holds the bridge. The master itself
  stays open — its only closer is the teardown SIGHUP path — and exit truth
  remains with the session's `waitpid` reap, never with the read side.

`qtermwidget6` remains a mandatory, version-pinned (`2.4...<2.5`) dependency
of the terminal target, linked **PRIVATE** into the static adapter so no
other module gains its headers or usage requirements.

## Consequences

- Keyboard, paste, and `sendTextToSession` bytes reach the child as real
  input; child output and echo render through the unchanged widget pipeline.
- The child's stdio is an ordinary blocking TTY; no descriptor-flag leak can
  break shells or pagers.
- Resize propagation is explicit application behavior instead of an
  upstream side effect, and the teardown guarantee gains a direct SIGHUP
  path (bridge master close) independent of widget disposal.
- One more platform surface (`posix_openpt`/`grantpt`/`unlockpt`/`ptsname_r`)
  lives in the Terminal module; the bridge is qtermwidget-free and is
  exercised by a real-PTY registered test without a display.
- ADR-0030's slave-forwarding decision is superseded; its remaining
  contracts (dependency confinement, exit-truth ownership, bounded
  process-group teardown) continue in force.

## Revisit when

- Upstream exposes a public master-side writer or an emulator-injection API
  that makes the second PTY unnecessary.
- A first-party need for multiplexed sessions or remote-terminal control
  changes the descriptor ownership model.
