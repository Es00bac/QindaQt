# ADR-0217: a panel may borrow keyboard focus, for exactly as long as a popup needs it

- **Status:** Accepted
- **Date:** 2026-09-19
- **Owners:** Shell runtime, Launcher
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaQt publishes every panel with layer-shell keyboard interactivity `None`
(`layer_shell_surface_backend.cpp`). That is what keeps typing in the window
the user is looking at: a focusable bar would sit in the compositor's focus
rotation and swallow keystrokes meant for an application.

The Meta key has to open the application launcher. KWin resolves a bare
modifier through `kwinrc`'s `[ModifierOnlyShortcuts]`, which invokes the
shell's `qindaqt_open_launcher` action (see
[Launcher](../shell/launcher.md)). Everything up to the applet works; the
applet then fails to show anything:

```
qt.qpa.wayland: Failed to create grabbing popup. Ensure popup … has a
transientParent set and that parent window has received input.
```

A Wayland popup that grabs input needs a serial from a real input event. A
pointer click on the panel button supplies one, which is why the mouse path has
always worked. A global shortcut supplies none, and a panel that can never be
focused never receives one either.

## Decision

A panel may borrow keyboard focus, and only as a loan around one popup.

1. **The loan is scoped to the popup.** `LauncherKeyboardFocusRelay` sets the
   hosting panel's interactivity to `OnDemand` and calls `requestActivate()`,
   waits for the panel to actually become active, and only then asks the
   controller to open the browser. The applet reports its popup closing
   (`LauncherAppletController::notifyBrowserClosed()`), which returns the
   panel to `None`. Grants and revokes stay balanced; a focused row asserts
   that across repeated cycles.
2. **The wait is bounded.** If focus never arrives within 400 ms the browser is
   opened anyway. A launcher that appears without a grab is better than a Meta
   key that does nothing, and the failure is then Qt's own warning rather than
   silence.
3. **The hosting window is resolved per press, from the scene.** The relay asks
   the window factory which live panel window actually contains a
   `launcherApplet` item. Panels are republished whenever outputs or the layout
   change, and a policy-denied applet must not report a host.
4. **Nothing else borrows focus.** This is not a general panel capability: no
   other applet, and no panel chrome, may raise interactivity. The default for
   every published panel stays `None`, and the surface backend still sets it.

## Consequences

- While the launcher's browser is open, the panel hosting it is focusable — it
  has to be, or the search field could not receive typed characters. Closing
  the browser (Escape, an outside press, or an activation) returns focus to the
  session.
- A crash between grant and close would leave that panel focusable until the
  shell restarts. The supervisor respawns the shell, and a republished panel is
  created `None` again, so the window is bounded by the shell's own lifetime.
- The same mechanism is what any future shortcut-opened applet popup will need;
  point 4 says that is a decision to make again, not a door left open.

## Revisit when

Wayland or KWin offers a way to create a grabbing popup from a shortcut
activation token, or a second applet needs the same loan and the policy is
worth generalizing.
