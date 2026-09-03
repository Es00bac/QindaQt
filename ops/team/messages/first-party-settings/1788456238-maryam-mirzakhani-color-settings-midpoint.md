# Color Settings route — midpoint (Maryam Mirzakhani)

- Time: 2026-09-03T11:23:58-06:00
- Worker: Maryam Mirzakhani (`maryam-mirzakhani`), branch
  `worker/color-settings-route`

Status: implementation complete; verification in progress.

Evidence so far (all run from the worktree, build root
`/home/cabewse/work_SPaC3/builds/qindaqt/color-settings-route`):

- `ctest -R '^qindaqt\.settings-color-'` Debug: 6/6 passed (model, apply,
  page wide/compact under `QT_FATAL_WARNINGS=1`, boundary, boundary-poison,
  installed-route with real relocation).
- `ctest -R '^qindaqt\.settings-'` Debug: 53/53 passed. Release: 53/53
  passed.
- Static gates: `./tools/validate-docs` exit 0; `mkdocs build --strict`
  exit 0; `./tools/check-source-shape` exit 0; `git diff --check` clean.
- `desktop.virtual.sandbox-unit` Debug passed; `desktop.virtual.package-contract`
  initially failed only because the desktop stage targets were not yet built
  in this fresh build root (missing `qindaqt-wm` binary) — now building them
  in both profiles before rerunning. Also appended the two color QML targets
  to `_qindaqt_desktop_targets` (parity with the Power block, required by the
  DesktopVirtual staging).

Material findings recorded in the owning page:

- The C1 import digest cannot be recovered by rescanning (discovery never
  reads profile bodies), so the route model retains the import result's
  SHA-256 lineage for profiles imported during its session; previously
  imported profiles persist an empty lineage, as the C1 document contract
  records.
- DisplayClient resets its snapshot on owner loss, so the route's "stale"
  truth is settings-side: Settings1 retains the last confirmed assignment
  document, which the page presents as stale with controls closed.
- Discovery runs only while the route is active (route hook mirroring the
  Bluetooth lease pattern) and roots derive from XDG standard locations, so
  sandboxed tests never read host ICC directories and an idle Settings
  process never scans.
