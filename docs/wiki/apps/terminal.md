# QindaQt Terminal

`qindaqt-terminal` is QindaQt's first-party terminal. S0 is intentionally one
complete ordinary desktop-client outcome: a single Qt 6 window that owns one
PTY session, runs the configured shell, renders UTF-8 output, accepts keyboard
input, supports bounded selection/copy/paste, reports exit and restart, and
guarantees child teardown. Tabs, profiles, search, links, GPU rendering, and
advanced VT behavior are explicit deferrals, not hidden claims.

Launch policy, session lifecycle, rendering adaptation, and presentation are
separate owners inside `src/apps/terminal`. The qtermwidget dependency and its
confined adapter are recorded in ADR-0030, whose slave-forwarding design was
superseded by
[ADR-0040](../adr/0040-own-terminal-child-pty-and-bridge-through-teletype.md)
—the application-owned child PTY bridge that is the current contract.

## Shell launch and the no-shell-string contract

A launch request is always argv: one absolute program path plus verbatim
arguments. Nothing is ever joined into a shell string, so a hostile argument
can become data for the resolved shell but never a second command. The pure
launch policy resolves the program from an explicit `--shell` value, then
`$SHELL`, then `/bin/bash`, and rejects values that are relative, missing,
directories, non-executable, oversized, or contain control characters.
`--arg` passes one verbatim argument and may repeat. Positional arguments are
rejected with exit code 2 before any window or session exists, so the CLI can
never be mistaken for shell-string syntax.

The child environment is derived, not inherited blindly. Entries with
malformed keys, newline-bearing values, or oversized entries are dropped, never
repaired. `TERM=xterm-256color` and `COLORTERM=truecolor` are always forced;
inherited values for those names never pass through. The effective character
set follows libc locale precedence `LC_ALL` > `LC_CTYPE` > `LANG`: the first
variable present in that order must select UTF-8, and when it does not, the
policy replaces exactly that variable with `C.UTF-8` (when none is present,
`LANG=C.UTF-8` is appended). A UTF-8 `LANG` therefore cannot mask a non-UTF-8
`LC_ALL`, because the rendering layer decodes child bytes as UTF-8 and a
non-UTF-8 effective child locale would be rendered wrong.

## Session lifecycle, exit truth, and teardown guarantee

The application owns the terminal child and its PTY. The rendering adapter
runs the shell itself (`setsid`, controlling TTY from the bridge PTY slave,
`execve` argv), so QindaQt—not the widget—owns `waitpid` exit truth and the
process-group identity captured at start. One session owns one PTY
generation; generations are never reused.

Exit reporting is typed: `exited (code N)`, `terminated by SIGxxx`, `exited
(status unknown)` when another reaper consumed the `waitpid` status, or a
bounded start-failure diagnostic shown in the status bar with QST danger or
warning colors. Restart tears the current generation down and starts a fresh
one; restarts are rejected while a shutdown is already in flight or while a
SIGKILL survivor is owned.

Teardown is a bounded escalation, not a hope:

1. The bridge PTY master closes (the kernel delivers `SIGHUP` to the child
   session).
2. After the close grace elapses, `SIGTERM` is sent to the exact captured
   process group — never to a bare PID, and only after the group leader is
   revalidated, so a recycled PID can never be signaled.
3. After the term grace, `SIGKILL` to the same group.
4. If the child somehow survives `SIGKILL`, the session reports a shutdown
   failure honestly, retains the backend and the captured process-group id,
   and refuses further close, quit, and restart attempts for that generation;
   `start()` also refuses to replace it.

Window close hides the window, runs the escalation, and only then quits the
application, so a surviving child can never be orphaned by an early exit. The
application wiring disables Qt's quit-on-last-window-closed default before the
first window is shown — hiding the only window must not end the event loop
while the escalation is running — and the sole quit path is a queued
connection that fires only after a clean shutdown. Closing during a pending
restart cancels the restart instead of launching a child that the quit would
immediately destroy, including when the close arrives while the restart's
teardown is already in flight; every non-refused close routes through the
session's `beginShutdown`, which is the restart cancellation in that state.
Bounds are injected values (default close 3 s, term 1 s,
kill 1 s; 20 ms poll) which makes the sequence deterministic in tests.

## Rendering adapter boundary

`qtermwidget6` is linked only by the terminal's rendering adapter, and only as
a private link dependency; no other module gains its headers, include paths,
or usage requirements, and tests never link it. Per ADR-0040 the adapter owns
a second, application-side PTY: the child's controlling TTY and stdio are the
bridge slave opened by path, so child stdio stays blocking and no descriptor
flag can leak; keyboard and paste bytes are written to the bridge master (the
only input direction); child output and line-discipline echo are read from the
bridge master and forwarded into a private duplicate of the widget's teletype
slave, which the widget's master reader feeds to the emulator; child winsize
is programmed explicitly from the live emulator grid on widget resize. The
widget transport is byte-transparent: the bridge already delivers
line-disciplined child output, so the adapter clears output processing
(`OPOST`) on its teletype duplicate with fail-closed verification, and a
start attempt fails with a typed diagnostic rather than rendering bytes a
second line discipline has mutated. The bridge read side is quiescent after
a terminal read condition (EOF, `EIO` after the last slave closes, or a hard
error): its notifier is disabled for the rest of the generation while the
master stays open, because Linux keeps a hung-up master readable forever and
the retained Exited session must not spin. Each
descriptor has exactly one writer, buffers are bounded (64 KiB) with
drop-newest backpressure, and the adapter keeps fork/exec, reaping, and view
disposal. `qindaqt-terminal` links the adapter; the support library with
policy, PTY bridge, session, and presentation links Qt and QST only, making
the boundary enforceable at link time.

## Keyboard and accessibility semantics

Every window command is a persistent top-level `QAction` with a stable object
name, Shift-modified terminal-safe shortcut, and window-shortcut context.

| Action identity | Default | Meaning |
| --- | --- | --- |
| `sessionRestartAction` | `Ctrl+Shift+R` | Tear down and start a fresh session |
| `editCopyAction` | `Ctrl+Shift+C` | Copy selection to clipboard |
| `editPasteAction` | `Ctrl+Shift+V` | Paste clipboard into the session |
| `editPasteSelectionAction` | `Ctrl+Shift+Insert` | Paste primary selection |
| `editSelectAllAction` | `Ctrl+Shift+A` | Select the whole buffer |
| `viewClearAction` | `Ctrl+Shift+K` | Clear display and scrollback |
| `fileQuitAction` | `Ctrl+Shift+Q` | Guaranteed-teardown close and quit |

No window action binds a plain `Ctrl+<letter>` readline sequence (`C`, `S`,
`Q`, `A`, `Z`, `X`, `V`, `R`, `K`, `W`): flow control and shell line editing
belong to the child program, and stealing them would be a functional
regression. Copy is enabled only while a selection exists; paste actions
deactivate safely when no generation is live, so a retained Exited buffer
never accepts paste. Select All publishes the adapter's real selection
availability — an empty buffer never enables Copy. The retained Exited view
keeps scrollback operations (Select All, Clear, and Copy of an existing
selection) available. The embedded view takes focus
when published, has `StrongFocus` policy, an accessible name and description,
and the window exposes its title, session status, and accessible status text.
Deep screen-reader bridge qualification stays a cross-application milestone
(QQ-006.09), not an S0 claim.

## QST-1 theme and appearance

The appearance adapter derives the complete window palette, interface font,
monospace terminal font, focus ring, and status colors from the public QST-1
boundary, exactly as the Text Editor does; `qinda-dark` is the launch default,
with `--theme` and `--theme-directory` selecting a validated schema-v1 theme
and `--check-theme` providing the packaging diagnostic that exits before any
window exists. The adapter renders the sixteen ANSI slots from public token
roles into a Konsole-format scheme document: red/green/yellow/blue map to
QST danger/success/warning/accent foregrounds, magenta maps to the accent's
subtle role (QST publishes no magenta hue), and the eight bright slots use one
mechanical lighten step because QST has no distinct intense roles. This is
bounded presentation adaptation; a full, settings-backed color-profile system
is a later slice and is not invented here.

The production adapter installs that document under a unique atomic
`.colorscheme` cache path, the suffix required by qtermwidget 2.4's custom-file
loader. Its eight bright groups use the upstream
`Color0Intense`..`Color7Intense` names. A real-adapter offscreen regression
renders the selected theme's terminal background and rejects qtermwidget's
synthetic LF-only selection for a pristine grid, so scheme and Copy
availability claims are not inferred from document generation or fake
backends.

## Desktop integration and verification

`org.qindaqt.Terminal.desktop` registers the ordinary Wayland application with
`Categories=Qt;System;TerminalEmulator;`, no `MimeType`, and
`StartupWMClass=qindaqt-terminal`. The installed `Terminal` component contains
the executable, the desktop entry, and the built-in theme data.

The focused selector is:

```sh
ctest --test-dir build/dev -R '^qindaqt\.terminal-' --output-on-failure
```

It covers hostile program/argument/environment resolution with UTF-8 byte
ceilings, effective UTF-8
locale precedence with a strict codeset oracle, forced `TERM`/`COLORTERM`,
real metadata-based executable checks, the real-PTY bridge (input direction,
output/echo capture, winsize, close, and read-notifier quiescence with
retained-master bounded liveness after the slave side disappears), the
session state machine (typed start
failures, exit-code versus signal versus unknown-exit publication,
duplicate-exit suppression), the teardown escalation sequence including
refusal to replace an unkillable generation, ownership retention with
close/quit/restart refusal while a survivor remains, close-cancels-pending-
restart through the session route and the production window route (Restart
then real close spawns no second generation),
forced destruction of a mid-shutdown session, restart generation
replacement, view-disposal ordering, the close/quit wiring contract (the
quit-on-last-window-closed flip, no early `aboutToQuit`, and the main-source
wiring binding), window action identity and action-state truth across
Running→Exited, readline-safe shortcuts, exit-status severity rendering,
accessibility and focus metadata, hostile-resize clamping, QST scheme
documents for all five themes, real-adapter custom-scheme rendering and blank
selection truth, desktop metadata, positional-argument
rejection, and staged installed metadata with installed-prefix theme
resolution. Every Widgets-linked row sets `QT_QPA_PLATFORM=offscreen`, so
the selector runs in display-less environments with no display variables
set. The installed and CLI rows
exit before any window or session exists.

The mandatory exact private-Wayland live lane additionally covers the real
shell's UTF-8 and ANSI rendering, keyboard-to-child byte flow, resize/SIGWINCH,
populated select/copy and paste, normal and signal exit truth, restart/close
teardown, first frame, and aggregate PSS; passing that lane is an integration
gate for this slice. Physical-display/GPU behavior and host-compositor
interaction remain outside S0.

## Bounded S0 deferrals

- Tabs and multiple sessions: one window owns exactly one session.
- Profiles, font/size settings, and Settings1 persistence: appearance derives
  from one `--theme` per launch only.
- Search, OSC-8 hyperlinks, click-to-open, and link tooltips stay disabled.
- The GPU/scrolling optimizations of the widget are upstream concerns; no
  rendering-performance claim is made.
- Advanced VT behavior beyond what the widget already provides (alternate
  screen integrations, sixel, reflow policies) is unqualified.
- A QindaQt-branded icon and global-menu export wait for later branding and
  application-shell slices.
