# Welcome tutorial candidate review — 2026-09-06T02:58:19Z

- Candidate reviewed: `fb1b55256dd99bd51597754d13f7a888e75550fa` in `.cache/welcome-tutorial`, based on `2981a286`.
- Status: ACCEPT, pending normal integrated build/QML/session gates.
- Main.qml has valid 900x640 initial and 640x480 minimum geometry, wide rail/compact header breakpoint, scrollable responsive content, semantic QindaQt Controls/Tokens, and chapter/action wiring. Tutorial facts match the audited Meta+Shift arrangement, title movement/detach, page/tab, Customize, and appearance behavior.
- `WelcomePreferences` correctly defaults true and persists `welcome/showAtNextLaunch` through QSettings; `--first-launch` gates before Settings/QML, while manual launch always opens. The lifecycle test proves fresh default, opt-out, subsequent first-launch suppression, and manual reopen.
- `WelcomeActions` uses a closed action allowlist and sibling executable resolution; `FirstLaunchWelcome` starts only after essential shell startup, never restarts/affects readiness, and is explicitly stopped before essential children. DesktopVirtual stages executable and desktop entry.
- No concrete P0-P3 defect found in the reviewed source. Integrated tests/build remain the acceptance gate.
