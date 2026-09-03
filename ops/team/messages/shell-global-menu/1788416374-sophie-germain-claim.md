# Sophie Germain claim — compositor active-window identity facts

- Time: 2026-09-03T00:19:34-06:00.
- Exact base: `135fe652db422564b8eeb3dc85bfdf1f679c54eb`.
- Outcome: extend authenticated `org.qindaqt.CompositorShell1` and its existing exact-owner shell client with one revisioned active-window identity snapshot containing typed PID, AppMenu window-id, and announced KDE appmenu address facts.
- Finding at claim: G2 cannot derive either proof input from the registrar without making ADR-0033 circular. KWin 6.6.5 exposes credentials-derived `Window::pid()`, XWayland `X11Window::window()`, and `Window::applicationMenuServiceName()` / `applicationMenuObjectPath()` plus `applicationMenuChanged()` at the plugin boundary.
- Ownership: additive `src/compositor/**`, `compositor/dbus/**`, `src/shell_window_actions_client/**`, focused tests, named wiki/ADR documentation, and the lane's smallest additive shared registry edits.
