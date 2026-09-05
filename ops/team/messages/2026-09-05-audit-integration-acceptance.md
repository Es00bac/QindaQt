# Manager integration acceptance — usability audit

The original audit baseline is `9728612046940b55d69f85c3811eb38a08a0963b`.
The selected repair series integrates independent candidates for customization,
popups/menu/environment, panel allocation/lazy loading, persistent launcher
settings, preferences, desktop controls and compact chrome. Candidate histories
remain preserved in their branches and worktrees.

## Verification

- Full system Qt/KWin Debug build: exit 0.
- Broad non-nested sweep: 640/643 initially passed. Candidate
  `5fcb2b754e2b9810aba82dce8bb6cce650326982` independently accepted and integrated
  at `997688db7d4bffb18a1cf565d2141d685308f042`; combined rerun of all three
  failures passes 3/3, exit 0. These repaired harnesses now link the actual
  desktop-controls static QML plugin; assertions were retained.
- Installed-service suite: 14/14, exit 0.
- Production stock resolver: ten profiles, 90 effective instances, 90 ready.
- Compact chrome focused geometry/hit/render/plan/plugin gates: 5/5.
- Private pointer tests assert process-local Hybrid grouping, native member
  detachment, two-page tabs, changed visible page after a shared-row click,
  larger group width after resize, and cleared ownership after tab detachment.
  Run exits 0 and cleanup reports no survivors. Executable exploratory helper
  and snapshots are retained under ignored `.cache/wm-visual/`.
- Live confirmed Settings1 theme update and supervisor shell restart preserve
  the saved Mac profile. Screenshots show live panel and notification colors.
- Outward Launcher/Bluetooth/Power popups and Escape were directly captured.
- Settings selected-navigation contrast focused test: 1/1.

## Limits and next work

This is Debug and private-nested evidence, not new Release or physical hardware
qualification. Task-list preferences, global-menu wrapping and shared notification
control styling remain in the audit backlog. Shared compositor/KDecoration colors
are not yet connected to live shell appearance. PSS optimization is deferred by
the user. No credentials were requested and no host installation was performed.

The initial failed compact test accidentally created a bridge-owned container;
it is not regression evidence. The subsequent failed key request used an arrow
name rather than the supported left mouse button. Both fixture errors were
corrected; the final test uses actual pointer events and Hybrid authority checks.

## Final composition cleanup

Candidate `7202c20686e07d6951f0c30a5c0d5cac2c0a866c` (including parent
`0e7dda4257cf6c7399d945382638a59117579e0a`) was independently accepted and
integrated through `7e28c807bcf625d604537fc60fcaad874d47acc2`.
The combined full incremental build and shell-runtime selector pass (6/6).
The normal source-shape command exits 0 with twelve decomposition warnings,
not zero warnings. The reviewer posted a correction to its original evidence.
The central runtime remains a composition root; appearance construction now
belongs to its existing token initialization translation unit, and the oversized
initialization function passes the enforced limit without suppression.

The first final display matrix failed all five rows on the same shelf-composition
validator. This is not accepted evidence; its actual state is under investigation
before rerunning the matrix.

The matrix failure was traced to applying QindaQt's smart-shelf requirement to
five intentionally different layout profiles. The first proposed fixture repair
`0543d91e` was blocked in manager/independent review: it still required a task-list
in GNOME's intentionally overview-only profile. Actual preset contracts, not a
single passing synthetic fixture, define the repaired matrix acceptance.

## Final matrix acceptance

Reviewed replacement `8be6f9c242c0034cadb338feed54a598a1bca6fe` is integrated
through `cbbcab3f094b9db43c922d460cd13b7c420e1ff1` with exact matching owned
files. The replacement was amended rather than a descendant; the integration
overlap was resolved from the reviewed candidate without touching other work.
The integrated full incremental build passes. Harness unit/syntax selector
passes3/3, and all five nested profile/resolution scenarios plus package fixture
pass6/6 (exit0,45.15seconds). The prior failing matrix is superseded by this
successful run, not omitted from the record.

The final test-only fixture extraction `c0492e0802f5df5bf225d7fa6ab7d48565dc14ab`
was independently accepted and integrated at `fe374d40`. It removes the new
function-length violation without changing fixture values or assertions. Final
normal source-shape, strict MkDocs, link validation and affected harness checks
are recorded in the manager logs under ignored `.cache/audit-final-*`.
