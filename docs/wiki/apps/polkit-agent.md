# Polkit authentication agent

`qindaqt-polkit-agent` is the dialog polkitd shows when an action needs a
password -- installing a package, changing a system setting, anything gated
by a polkit action. It is not launched by the user; the session supervisor
starts it, and it stays resident for the whole session, showing its dialog
only while a request is open. This is [ADR-0290](../adr/0290-native-polkit-agent-and-single-agent-rule.md).

## Why it exists

Before this agent, two different polkit agents raced to register at every
login -- the supervisor's KDE candidate and XDG autostart's polkit-gnome
entry -- and polkitd kept whichever registered first. This agent is the
only one QindaQt starts, and the session's own autostart catalog treats
every other distribution agent's `.desktop` entry as superseded, so exactly
one agent registers.

## What it shows

When polkitd calls this agent's `initiateAuthentication`, its dialog opens
as a layer-shell overlay on the output under the pointer, with a token-
coloured scrim across that output and an exclusive keyboard grab, so
keystrokes cannot land anywhere else while a password is being typed. It
shows, in order:

- the title "Authentication required" and polkit's own message for the
  action;
- the requesting application's icon and name, resolved from the requesting
  process's pid (`/proc/<pid>/exe` and `comm`) and, where a matching
  `.desktop` file is cheap to find, that entry's `Name`/`Icon`; otherwise the
  program's path;
- an identity chooser, shown only when polkit offered more than one
  identity to authenticate as -- the current user is preferred when polkit
  lists them, otherwise the first identity; an identity is shown as
  "Full Name (login)", or "Administrator (root)" for uid 0;
- a response field, focused on open, whose accessible name and echo mode
  follow whatever PAM is currently asking for -- usually the password, but a
  later multi-step prompt (an OTP, say) replaces it in place;
- an info/error line for PAM's own `showInfo`/`showError` text and for a
  failed attempt's "That password didn't work. Try again.";
- a "Details" disclosure with the action id, the action's vendor (where
  polkit's `details` carry it), and the requesting program's path;
- Cancel and Authenticate.

Enter authenticates and Escape cancels. A wrong password clears and
refocuses the field and starts a fresh attempt automatically, with the
dialog staying open; this agent never enforces a retry limit itself --
polkitd stops the dialog by cancelling the request once it gives up. A
second `initiateAuthentication` call while one dialog is open is queued, not
dropped, and takes over the moment the first is resolved. The password is
never logged, never kept in any model past the call that uses it, and
cleared from the field after every attempt, success or failure alike.

The platform role is provided through the public [AuthenticationOverlay](../architecture/authentication-overlay.md) module. Initialization precedes QGuiApplication and configuration precedes native window creation/show; policy and presentation remain owned by this agent.

## Appearance and output

Surfaces, spacing, radii, type, and colour all come from the design tokens
the Appearance route publishes, the same Settings1 subscription and theme
controller every first-party app uses, so the dialog follows the session's
theme, including dark mode. The agent chooses the output under the global
cursor position and falls back to the primary output when Qt reports no
position. Wayland may report `(0, 0)` instead, which can choose the first
output; pointer placement across multiple outputs remains to be qualified in
a live session.

## Single-agent rule

- `defaultPolkitAgentCandidates()` (`src/session_supervisor`) names only
  this agent's install path; there is no KDE or other distribution
  fallback. `--polkit-agent` overrides the path and `--no-polkit-agent`
  suppresses the agent outright, both unchanged from before this agent
  existed.
- The session autostart catalog (`src/session_autostart`,
  [ADR-0247](../adr/0247-run-xdg-autostart-in-the-session-supervisor.md))
  marks a documented table of distribution polkit-agent `Exec`/`TryExec`
  basenames ineligible -- superseded by this agent -- and logs each skipped
  entry, regardless of that entry's own `NotShowIn`/`OnlyShowIn`. With
  `--no-polkit-agent`, they retain normal eligibility; other entries are
  unaffected.
- The process selects LayerShellQt before constructing QGuiApplication. It
  refuses non-Wayland platforms or a missing layer surface and never falls back
  to an ordinary top-level password window.

A registration conflict with polkitd (another agent already holds the
  session's slot) exits this agent with status 2 and a stderr line, never a
  crash loop; the supervisor's shared one-restart budget for optional
  children already bounds the retry.

## Evidence

- `qindaqt.polkit-agent-identity` (unit): identity label formatting and the
  current-user/first-identity preference.
- `qindaqt.polkit-agent-attempt-controller` (unit): attempt, retry, cancel,
  and cancel-from-polkit transitions.
- `qindaqt.polkit-agent-request-queue` (unit): a second request queues
  while one is open and activates when it completes; `cancelActive`
  promotes the next queued request.
- `qindaqt.polkit-agent-requester-resolver` (unit): pid-to-program
  resolution against a fake `/proc` root and applications directory,
  including the honest-empty case for an unreadable pid.
- `qindaqt.polkit-agent-dialog-qml` (offscreen QML): accessible names,
  Enter/Escape, the error line, the identity chooser hidden for one
  identity, the password field cleared after a failed attempt, and refusal to
  configure an authentication window on an unsupported platform.
- `qindaqt.session-polkit-agent-selection` and
  `qindaqt.session-autostart-catalog` (unit, `src/session_supervisor` and
  `src/session_autostart`): the single-candidate default and the autostart
  basename skip table.

Registering against a real polkitd, and the full dialog flow in an installed
session, remain owner-observed evidence: `pkexec true` in an installed
QindaQt session, then a wrong password, then the right one.
