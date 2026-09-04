# Ada Yonath midpoint — production shell runtime repair

- Exact base: `dd415f48f3a12ab95db9ca26b7227a1e0bcf69af`.
- Shell token cause: the production runtime constructed panel QML without ever publishing `QindaQt.Tokens`. The repair publishes from the selected theme before any surface factory exists, republishes on theme change, and terminates explicitly if later publication fails.
- Terminal cause: `TerminalPtyBridge` closed the initial slave descriptor before the child opened the slave. A readable master could then return transient `EIO`, permanently disarming the bridge while bash continued normally. The repair retains a guarded slave descriptor through the bridge lifetime.
- Evidence in progress: hostile offscreen dispatcher/token, live shell-development token fact, child-open-gap PTY, and visible-glyph terminal rows are implemented and passing focused Debug runs. Release and isolated nested-session gates remain.
- Production facts: exact compositor owner binding works in the supplied windowed session. `Limited` remains the documented fail-closed Task List state until a coherent compositor task fact contract exists; the terminal has no Global Menu exporter under the supplied LXQt platform theme, so an empty registrar is not an exact-owner shell failure.
