# Lane S (Opus) — native QindaQt screenshot tool (replaces Spectacle)

Worker name `claude-screenshot`, voice seat `claude-helper-two`, speech name "screenshot tool".
Worktree: `cd ~/work_SPaC3/container-wm && git worktree add ~/work_SPaC3/container-wm.worktrees/screenshot -b feature/qindaqt-screenshot hub/main`
Push to `hub`. ADR number reserved: **0289**. This is slice "screenshot tool" of M4 in
`docs/plans/2026-09-28-plasma-free-qindaqt.md` (hub branch `plan/plasma-free`), pulled forward.

Owner: "We obviously need a new screenshot tool." Spectacle segfaults (coredump 2026-09-28 16:54 on
logout) and is a Plasma dependency QindaQt is removing.

MVP scope (ship this first, tightly):
1. App `src/apps/screenshot` → `qindaqt-screenshot` (QML + QindaTK/QindaQt Controls, the app-shell
   pattern the other first-party apps use). Capture through KWin's ScreenShot2 D-Bus interface,
   exactly as `src/shell/runtime/kwinscreenshotpreviewport.cpp` does (authorization: the desktop
   file's `X-KDE-DBUS-Restricted-Interfaces`). Modes: rectangular region (full-screen selection
   overlay across all outputs, Escape cancels, keyboard nudging), full screen (all outputs / current
   output), active window, window under the pointer; delay (0/3/5/10 s); include pointer; optional
   window decorations.
2. After capture: a small result window with the image, **Copy** (clipboard), **Save** (default
   `XDG_PICTURES_DIR/Screenshots/Screenshot_YYYY-MM-DD_HH-MM-SS.png`, never overwrite), **Save As…**,
   **Open**, and a QindaQt notification with Open/Copy/Show in folder. A no-UI command-line mode
   (`--region|--fullscreen|--window|--active --copy|--save [path]`) for shortcuts and scripts.
3. Shortcuts via the existing shell/global-shortcut path QindaQt uses today: Print = region,
   Shift+Print = full screen, Alt+Print = active window, Meta+Shift+S = region (common muscle memory).
   Settings → Shortcuts lists them.
4. Packaging: install app, desktop file, icon (`org.qindaqt.Screenshot`, the icon catalog already
   has a camera mark; add an alias if needed), and remove `kde-plasma/spectacle` from what QindaQt
   itself needs (note for the manager: the overlay ebuild RDEPEND line, and the release contract in
   `tools/check-release-contract`, must drop spectacle; update both in this branch where they live in
   the repo).
5. Tests: capture-request builder and filename policy (unit), command-line parsing, QML result page
   (offscreen), and a nested-session capture of a known window if the testing harness supports it
   cheaply. Docs: an app wiki page, ADR-0289, handbook shortcut list.

Out of scope for this lane: annotation tools and the xdg-desktop-portal Screenshot backend (list them
as follow-up slices in your report). Build on qinda with `-j8 -l24`, run focused tests, commit, push,
report (commits, files, tests with counts, what needs a live check). Keep token use low.
