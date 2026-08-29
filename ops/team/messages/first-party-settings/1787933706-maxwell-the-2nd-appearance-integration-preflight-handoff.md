# Maxwell the 2nd — Appearance integration preflight handoff

- Time: 2026-08-28T16:15:06Z
- Outcome: complete, read-only
- Public target: `ab36cd8`
- Accepted candidate: `d71fac4a2c7e8944822b3185aee5bb43acd455c7`
- Merge base: `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- Synthetic merge tree: `f26135ae9af2cd51cee66b7d99e2d0603f809090`

## Exact collisions

`git merge-tree --write-tree --messages ab36cd8 d71fac4` reports exactly four
content conflicts:

1. `docs/wiki/adr/index.md`: retain ADRs 0026, 0027, **0028**, 0029, and 0041
   in numeric order, plus public's reserved-gap explanation.
2. `docs/wiki/architecture/module-boundaries.md`: retain public AppShell,
   Power1, brightness-model, and File Manager rows plus candidate Appearance.
3. `docs/wiki/index.md`: retain all public AppShell/File Manager/Power links and
   add the candidate Appearance route link.
4. `mkdocs.yml`: retain every public nav item and add Appearance plus ADR-0028.

`docs/wiki/development/testing-harness.md`, `src/CMakeLists.txt`, and
`tests/CMakeLists.txt` auto-merge cleanly and additively. The synthetic result
keeps Appearance before Settings Center and keeps public AppShell, Power,
brightness, and File Manager registrations. No other textual or add/add
conflict exists.

## Required semantic repair before calling the combined tree qualified

The public `DesktopVirtual` component in
`tests/session/DesktopSessionTests.cmake` stages `qindaqt-settings` and themes,
but not the new Appearance QML module, Tokens, or Controls. Candidate
`d71fac4` makes `qindaqt-settings` directly link/load Appearance and Tokens and
assigns their complete installed payload to `SettingsAppearanceRuntime`.
A `DesktopVirtual`-only stage can therefore retain a package-contract-visible
Settings executable that cannot start in the private desktop. Preserve the
existing `DesktopVirtual` boundary by either staging both components or
duplicating the exact Appearance/Tokens/Controls transitive runtime into
`DesktopVirtual`; then rerun package and private boot evidence.

This does **not** break File Manager component staging: its CMake already
duplicates Themes, Tokens, Controls libraries/plugins/qmldir/typeinfo, and all
Qt-derived Controls QML paths into `FileManager`. AppShell's installed-consumer
row stages exact build artifacts itself. A component-less full install also
retains all rules.

## Combined-tree gate order

1. Resolve only the four conflicts above additively; run `git diff --check` and
   confirm the seven shared registry/docs files contain both sides.
2. Configure the integrated tree, then build serially: shared Tokens/Controls,
   Appearance and `qindaqt-settings`; the six Appearance/migration test
   executables; `qindaqt-file-manager` and its four model executables; AppShell
   and its three executables; finally `qindaqt-desktop-session-probe` after the
   DesktopVirtual staging repair.
3. Run Appearance exactly:
   `^qindaqt\.(appearance-(values|preview|settings-model|page)|settings-app-(offscreen|rejects-unknown-route|desktop-identity|route-construction|installed-routes)|settings-migration)$`.
4. Run public app regressions exactly:
   `^qindaqt\.(file-manager-(navigation-history|local-lister|launch-intent|navigation-controller|desktop-metadata|cli-rejects-multiple-paths|cli-rejects-non-folder|installed-runtime)|app-shell-(action-registry|coordinator|surface-offscreen|source-policy|installed-consumer))$`.
5. Because candidate changes shared install rules, run
   `qindaqt.controls-installed-import`,
   `qindaqt.design-tokens-installed-cpp-consumer`, and
   `qindaqt.theme-formats`, then the complete registered Controls/Tokens/Themes
   selectors if any focused row fails.
6. Run serial, non-live desktop gates
   `^desktop\.virtual\.(sandbox-unit|package-contract)$`. Only after a manager
   allocates the private-runtime lane, run the documented exact
   `desktop.virtual.boot.1080p` row; never use the host desktop/session.
7. Finish with source-shape, docs validator, `mkdocs build --strict`, link
   checker, and the current public combined selector before publishing.

No product file, branch, worktree index, desktop/session, input, or user
configuration was changed by this preflight.
