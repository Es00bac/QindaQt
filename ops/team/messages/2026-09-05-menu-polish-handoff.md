# First-party menu polish handoff

- Worker: Barbara Liskov
- Exact base: `5771c97644db5e48e622ab71154e4155c10c6714`
- Branch/worktree: `codex/polish-menu`, `/home/cabewse/work_SPaC3/container-wm/.cache/polish-menu`
- Requested action: independent review of the exact candidate commit, then manager integration and private visual matrix.

The candidate adds a per-endpoint QindaQt host acknowledgment to the existing registrar. A live renderer lease plus authenticated and canonically decoded provider tree records the acknowledgment. AppShell hides File Manager, Terminal, and Text Editor local menus only after the exact registrar owner confirms their endpoint. Foreign registrars, absent profiles/renderers, registrar replacement, endpoint withdrawal, registration/export failure, and unavailable replies retain or restore the local menu. Successful acknowledgments remain cached while a renderer lives, avoiding content-height jumps on ordinary focus changes.

Verification:

- Focused build of changed libraries, all three executables, and focused test binaries: exit 0.
- `ctest` AppShell export, global-menu access, registrar private bus, and transport composition rows: 4/4 passed.
- `qindaqt.global-menu-applet-qml-offscreen`: passed after explicitly building its plugin.
- `tools/validate-docs`: 168 documents/navigation validated.
- `tools/check-source-shape`: passed; exporter split below decomposition-review threshold (only pre-existing warnings remain).
- `git diff --check`: passed.
- `mkdocs build --strict`: unavailable (`mkdocs` is not installed on this host).

Bounded caveat: manager still owns the integrated private visual matrix; this worker did not run a live nested desktop session.
