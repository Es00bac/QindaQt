# QindaQt Terminal

`qindaqt-terminal` is QindaQt's first-party terminal. S2 is an ordinary Qt 6
desktop client with up to eight independent tabs in one window. Every tab owns
the complete S0 PTY/child/teletype lifecycle, may select a validated profile at
creation, and participates in teardown-first close and quit. User profiles,
the default profile, and the tab-restore policy are persisted through the
public Settings1 client. S2 adds bounded per-session scrollback search and
confirmed URL/local-path handling. The window opts its AppShell catalog into
the first-party global-menu export through the shared AppShell composition
entry; GPU qualification and
advanced VT behavior remain explicit deferrals, not hidden claims.

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
2. After the close grace elapses, `SIGTERM` is sent to the exact process group
   captured when the `setsid()` child was its leader — never to a bare PID.
   Reaping that leader does not release the captured group id or prevent this
   signal, because an inherited descendant may still own the group.
3. After the term grace, `SIGKILL` is sent to the same retained group.
4. Clean shutdown is published only after a `killpg(pgid, 0)` probe and Linux
   `/proc` process-group scan establish that no member, including an orphaned
   zombie awaiting its new parent's reap, remains. A member surviving the kill
   grace or an indeterminate emptiness check reports shutdown failure honestly,
   retains the backend and captured group id, and refuses further close, quit,
   restart, or replacement for that generation.

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

## Multiple sessions and tab titles

One `TerminalSessionCollection` owns at most eight sessions. A ninth creation
is refused without disturbing any existing child. Each accepted session gets a
fresh backend, PTY, bridge, child process, copied profile value, and tab. Close
Tab applies the S0 refusal/cancel rules to that session; closing the last tab is
quit intent and therefore takes the same close-all path as File > Quit or the
window decoration. Close-all starts every teardown, removes cleanly terminated
sessions, retains any SIGKILL survivor, and refuses application quit while a
survivor remains. Collection destruction synchronously invokes every retained
session's forced-destruction guard, including process-exit teardown.

Child-published tab titles are presentation data, never authority. Controls
and Unicode format characters are removed, whitespace runs collapse, unpaired
surrogates are dropped, and the result is capped at 128 UTF-16 code units
without splitting a surrogate pair. Empty results use `Session N`. The tab
strip has the accessible name `Terminal tabs`, exposes standard PageTabList /
PageTab roles, and is keyboard-operable through the persistent actions below.

## Profiles and Settings1 persistence

The immutable `builtin-default` profile is always available. At most 16 user
profiles may be persisted. A profile contains a stable safe identifier; a
trimmed printable name (maximum 64 characters); an optional absolute shell
program and at most 64 verbatim arguments under the existing launch-policy
byte limits; an optional font family and either the theme font size or 6–48
points; a safe QindaQt theme identifier; 0–100,000 scrollback lines; and a
`silent` or `audible` bell policy. Silent profiles strip BEL from child output;
audible profiles pass it to the renderer. A user profile never changes the
shell contract: its program and argv go through `TerminalLaunchPolicy`, never
through a shell string, and an invalid or unresolvable profile is refused. The
line-oriented profile editor preserves every unchanged argv element exactly,
including leading, interior, trailing, and sole empty arguments.

The terminal reads and writes this exact Settings1 scope:

| Key | Type/default | Meaning |
| --- | --- | --- |
| `services.terminalProfiles` | string / `[]` | Canonical JSON array of validated user profiles |
| `services.terminalDefaultProfile` | string / `builtin-default` | Profile copied into new sessions |
| `services.terminalRestoreTabs` | Boolean / `false` | Policy allowing a future saved tab inventory to be restored |

The three values form one logical draft but use the public v1 client's
single-key writes in the fixed table order. Each commit waits for the automatic
fresh snapshot before the next write, so no stale base revision is reused. An
authoritative conflict aborts remaining writes and requires an explicit
re-apply. Timeout, owner loss, or bus loss makes the in-flight result uncertain
and it is never replayed. Missing, malformed, incomplete, or wrong-typed
snapshots are rejected wholesale. Before a valid baseline and whenever the
transport or Settings1 authority is lost, new sessions use built-in defaults;
existing sessions keep the profile value copied at creation.

The Manage Profiles dialog does not close when Apply merely starts. It disables
the draft while the asynchronous sequence is pending, closes only after all
three keys are confirmed applied, and otherwise keeps the unchanged draft open
with a bounded accessible per-key result. The same complete result remains in
the window status surface: conflict requires review and explicit re-apply,
confirmed rejection names failed and not-attempted keys, and transport loss or
timeout is labeled uncertain and explicitly not replayed.

Session content, scrollback bytes, child environment, argv history, titles,
process identifiers, and tab inventory are never persisted. Consequently S1
persists the restore *policy* but deliberately has no content-bearing inventory
to restore yet; startup opens one tab using the confirmed default profile (or
the built-in default after a definitive Settings1 failure).

## Per-session scrollback search

Each session owns volatile find text, case sensitivity, regex mode, current
match/result announcement, and find-bar visibility. `Ctrl+Shift+F` opens the
non-modal in-window bar, `F3` and `Shift+F3` traverse with wrap, and Escape
clears renderer highlights, hides the bar, and returns focus to that session
without discarding its logical match position. A later `F3` or `Shift+F3`
resumes in the requested direction from that position. Switching tabs restores
the selected session's complete volatile bar and accessible result without
copying or announcing another tab's state. Search text is never sent to
Settings1 or any persistence surface.

The qtermwidget-free admission policy limits patterns to 256 UTF-16 code units,
snapshots to 4 MiB, and reported matches to 10,000. Literal search is always
escaped. Regex mode supports literals, classes, anchors, alternation, groups
that are not quantified, one variable repetition on a non-group atom, up to
eight optional atoms, and exact repetitions no greater than 32. It rejects
lookarounds/extensions, backreferences, quantified groups, multiple variable
repetitions, empty-string matches, invalid syntax, controls, and unpaired
surrogates before qtermwidget's synchronous search is invoked. This closed
subset is intentional: admitting general hostile patterns would permit
GUI-thread denial of service.

An accepted query uses the confined adapter's pinned qtermwidget 2.4 SearchBar
surface. The adapter enables its all-match renderer highlight and uses its
current-match selection/scroll behavior; the application UI never includes a
qtermwidget header or inspects an emulator object. Accessible status says
`Match N of M`, `No matches`, or the bounded refusal reason and publishes a Qt
accessibility name-change event. Match truth therefore does not depend on
highlight color.

## Visible links and confirmed opening

The adapter derives at most 256 link presentation values from the current live
screen tail, capped at 512 KiB. Detection accepts explicit `http://` and
`https://` URLs plus absolute local paths, including the root path `/`;
`file://` URLs are explicitly outside the admitted set. Each target is capped
at 2,048 UTF-16 code units; Unicode control and format characters (including
bidi controls and zero-width joiners) plus unpaired surrogates are removed,
terminal punctuation and unmatched closing delimiters are excluded, and
quoted absolute paths may contain spaces. URL/path text is otherwise never
normalized: Unicode homoglyphs, IDNs, and punycode remain exactly as printed,
and the tooltip says that hostname spelling was not normalized.

Nothing auto-activates. Copy and Open re-read the current viewport immediately
before acting; if the traversed selection disappeared or changed, that request
only refreshes presentation truth and neither copies, confirms, nor dispatches
the stale target. The fixed AppShell/local catalog includes Select
Previous Link, Select Next Link, Copy Link, and Open Link; the terminal context
menu presents the same actions, and their Shift-modified shortcuts provide
bounded keyboard traversal. Selection announces `Link N of M` and the exact
display text in the accessible session status. Copy writes only that exact
control-free target.

Open first shows a plain-text confirmation naming the exact URL or path. After
confirmation, an injected spawner receives the resolved absolute `xdg-open`
program and exactly one argv element; no shell string or interpolation exists.
Production intentionally uses `QProcess::startDetached`: the desktop opener is
a dispatch request with no terminal PTY or session job to own, and the selected
handler has an independent lifetime. Terminal teardown therefore neither
abandons a child session nor kills an application the user chose to open.
Tests inject recording confirmation/spawn seams and never run a real opener.

## Rendering adapter boundary

`qtermwidget6` is linked only by the terminal's rendering adapter, and only as
a private link dependency; no other module gains its headers, include paths,
or usage requirements. Only the dedicated production-adapter regression links
that adapter; support-library tests remain independent of qtermwidget. Per
ADR-0040 the adapter owns a second, application-side PTY: the child's controlling TTY and stdio are the
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
policy, PTY bridge, session, search/link values, and presentation links Qt and
QST only, making the boundary enforceable at link time. The S2 adapter public
boundary contains only typed search and link values, never qtermwidget types.

## Keyboard and accessibility semantics

Every window command is a persistent top-level `QAction` with a stable object
name, Shift-modified terminal-safe shortcut, and window-shortcut context.

| Action identity | Default | Meaning |
| --- | --- | --- |
| `tabNewAction` | `Ctrl+Shift+T` | Open a tab with the confirmed default profile |
| `tabCloseAction` | `Ctrl+Shift+W` | Close the active tab, or close-all when it is last |
| `tabNextAction` | `Ctrl+Shift+Right` | Select the next tab, wrapping at the end |
| `tabPreviousAction` | `Ctrl+Shift+Left` | Select the previous tab, wrapping at the start |
| `tabMoveLeftAction` | `Ctrl+Shift+Alt+Left` | Move the active tab left |
| `tabMoveRightAction` | `Ctrl+Shift+Alt+Right` | Move the active tab right |
| `profileManageAction` | `Ctrl+Shift+P` | Edit profiles and persistence policy |
| `sessionRestartAction` | `Ctrl+Shift+R` | Tear down and start a fresh session |
| `editCopyAction` | `Ctrl+Shift+C` | Copy selection to clipboard |
| `editPasteAction` | `Ctrl+Shift+V` | Paste clipboard into the session |
| `editPasteSelectionAction` | `Ctrl+Shift+Insert` | Paste primary selection |
| `editSelectAllAction` | `Ctrl+Shift+A` | Select the whole buffer |
| `viewClearAction` | `Ctrl+Shift+K` | Clear display and scrollback |
| `viewFindAction` | `Ctrl+Shift+F` | Open the active session's find bar |
| `viewFindNextAction` | `F3` | Select the next match with wrap |
| `viewFindPreviousAction` | `Shift+F3` | Select the previous match with wrap |
| `linkNextAction` | `Ctrl+Shift+L` | Select the next visible link with wrap |
| `linkPreviousAction` | `Ctrl+Shift+Alt+L` | Select the previous visible link with wrap |
| `linkCopyAction` | `Ctrl+Shift+Y` | Copy the exact selected link target |
| `linkOpenAction` | `Ctrl+Shift+O` | Confirm and open the exact selected target |
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
(QQ-006.09), not an S2 claim.

The same fixed commands are projected through `QindaQt.AppShell 1.0` as
`session.*`, `edit.*`, `view.*`, `link.*`, and `file.quit` action identifiers.
External activation is routed back to the corresponding local `QAction`, so a
global-menu activation cannot bypass local enablement or lifecycle policy.
After the window is shown, the executable composes the shared first-party
exporter `QindaQt::AppShell::MenuExport::composeFirstPartyMenuExport` with its
coordinator, the window's platform `QWindow`, and its session-bus connection;
the composition, lifecycle, and fail-closed rules are owned by
[the global-menu page](../shell/global-menu.md). A missing session bus or
registrar leaves the export disabled/waiting, and the local `QMenuBar` stays
visible and authoritative.

## QST-1 theme and appearance

The appearance adapter derives the complete window palette, interface font,
monospace terminal font, focus ring, and status colors from the public QST-1
boundary, exactly as the Text Editor does; `qinda-dark` is the launch default,
with confirmed Settings1 theme and color-scheme changes updating the running
window through [ADR-0079](../adr/0079-resolve-first-party-appearance-from-settings.md).
An explicit `--theme` locks a validated schema-v1 theme and `--theme-directory` extends discovery,
and `--check-theme` providing the packaging diagnostic that exits before any
window exists. The adapter renders the sixteen ANSI slots from public token
roles into a Konsole-format scheme document: red/green/yellow/blue map to
QST danger/success/warning/accent foregrounds, magenta maps to the accent's
subtle role (QST publishes no magenta hue), and the eight bright slots use one
mechanical lighten step because QST has no distinct intense roles. This is
bounded presentation adaptation; terminal profiles remain the persistence
authority for their explicit per-profile choices.

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
the executable, desktop entry, built-in theme data, and AppShell's linked
AppShell/Controls/Tokens backing libraries. `qtermwidget6`
remains an external dynamically linked package dependency. The staged metadata
gate resolves that dependency from the exact CMake-imported library file while
clearing ambient loader and theme roots; a build-tree RPATH or caller-specific
`LD_LIBRARY_PATH` cannot satisfy the installed-prefix proof.

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
duplicate-exit suppression), the teardown escalation sequence plus a real
HUP/TERM-immune descendant that outlives its reaped group leader and must be
killed before clean completion, including unconditional fixture cleanup;
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
selection truth; bounded multi-session creation, movement, close-all, and
forced destruction; title sanitization; hostile profile values, canonical
round trips, and unchanged empty-argument preservation; Settings1 baseline,
sequential apply, conflict, fail-closed loss, uncertain no-replay behavior, and
the production Manage Profiles modal remaining visible, enabled, and
accessibly descriptive after conflict, rejection, transport loss, owner loss,
or timeout while the all-applied control closes it; tab shortcuts,
traversal/movement, and
PageTab accessibility under `QT_FATAL_WARNINGS=1`; AppShell catalog and local
activation routing; desktop metadata; positional-argument
rejection, and staged installed metadata with installed-prefix theme
resolution. The global-menu export slice adds real-process private-bus rows
under the shared selector documented in
[the global-menu page](../shell/global-menu.md): exact-identity export with
one shell activation and provider-exit clearing, mismatched PID/window
variants, registrar-absent late binding, and hostile-registrar fail-closed
rebinding. S2 adds literal/case/regex policy, hostile-regex admission timing,
match/no-match/wrap and focus return through a fake adapter, deterministic
real-PTY search and current-selection highlighting through the production
adapter, per-session volatile find state, link punctuation/quote/parenthesis/
control/overlength/homoglyph cases, exact recording-spawner argv and
confirmation, context-menu/AppShell routing, and qtermwidget include poison.
The combined search/link UI row also sets `QT_FATAL_WARNINGS=1`. Every
Widgets-linked row sets `QT_QPA_PLATFORM=offscreen`, so
the selector runs in display-less environments with no display variables
set. The installed and CLI rows
exit before any window or session exists.

The S0 milestone separately required an exact private-Wayland live lane for
the real shell's UTF-8 and ANSI rendering, keyboard-to-child byte flow,
resize/SIGWINCH, populated select/copy and paste, normal and signal exit truth,
restart/close teardown, first frame, and aggregate PSS. This S2 worker does not
rerun or extend nested-session evidence. Physical-display/GPU behavior and
host-compositor interaction remain outside S2.

## Bounded S2 exclusions

- The restore-policy flag is persisted, but tab inventory and terminal content
  are intentionally not; startup restores no prior session bytes or argv.
- OSC-8 semantic hyperlinks are not interpreted; S2 detects only printed
  `http(s)` text and absolute local paths, and never click-to-opens them.
- The GPU/scrolling optimizations of the widget are upstream concerns; no
  rendering-performance claim is made.
- Advanced VT behavior beyond what the widget already provides (alternate
  screen integrations, sixel, reflow policies) is unqualified.
- A QindaQt-branded icon waits for a later branding slice. The global-menu
  export is composed through the shared first-party AppShell entry and proven
  by private-bus rows only; an installed nested-session qualification remains
  unbuilt.
- Whole-application assistive-technology proof, the nested screenshot matrix,
  and physical display/input qualification remain later integration gates.
