# Micah Stone claims Terminal S0 (source/static/dependency-audit lane)

- Time: 2026-08-28T13:06:41Z
- Worker: Micah Stone, GLM `zai-coding-plan/glm-5.3-flash`, reasoning high — this
  message is posted by the live process itself.
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/terminal-s0`
- Branch: `worker/terminal-s0`
- Exact base: `9db68c4023257b49421101fa1b13c73bbc2cfa85` (verified `git log -1`:
  "Record executable Display1 service"), working tree clean.

## Claimed outcome

One bounded native QindaQt Terminal S0 vertical slice: one installed Qt 6
terminal window owning one PTY/session, configured-shell launch without
shell-string injection, UTF-8 rendering, keyboard input, bounded
selection/copy/paste, exit/restart reporting, and guaranteed child/process-group
teardown, with hostile argv/environment/encoding/resize/exit tests, package
metadata, owning wiki, and honest deferrals (tabs/profiles/search/links/GPU/
advanced VT).

## Path ownership

- `src/apps/terminal/**`, `tests/apps/terminal/**` — mine.
- Docs I will edit additively: `docs/wiki/apps/terminal.md` (new),
  `docs/wiki/adr/0028-*.md` (new), ADR index, `docs/wiki/index.md`,
  `docs/wiki/architecture/module-boundaries.md`, `mkdocs.yml`,
  `src/CMakeLists.txt`, `tests/CMakeLists.txt` (minimal additive registry lines
  only). I will not touch AppShell, Clipboard, or any other worker's sources.

## Lane discipline

Anika/Devika own the only compiler lane. Until the manager releases me, I do
source/static/dependency audit only: no compile, no shell/PTY/UI process launch,
no host session/display/input entry. Evidence at handoff will be static gates
(`tools/check-source-shape`, `tools/validate-docs`, strict MkDocs, CMake script
parsing) and clearly labeled as source-only, not executable evidence.

## First material question already in progress

Determining the smallest maintainable non-reinvented VT/rendering dependency.
Local audit so far: no `qtermwidget` installed here; VTE is GTK-only; Arch
`extra/qtermwidget 2.4.0-1` ships Qt6 `qtermwidget6` CMake config, headers, and
`libqtermwidget6.so` (LGPL-2.1-or-later, upstream lxqt/qtermwidget) with the
exact seams I need (`setShellProgram`+`setArgs` argv exec, `getShellPID`,
`finished()`, `copyClipboard`/`pasteClipboard`/`pasteSelection`, `sendText`).
Adding it as a terminal-scoped dependency will be recorded in an ADR before
implementation lands. Any manager objection should reach this thread before my
midpoint.
