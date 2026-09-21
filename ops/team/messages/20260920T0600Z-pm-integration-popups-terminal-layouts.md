# PM integration — panel popups, start panel, terminal, layouts, applets

- When: 2026-09-20T06:00Z
- Role: Program Manager, direct implementation (no worker lanes on this wave).

## Integrated on main

| Commit | Outcome |
| --- | --- |
| `9d199663` | One owner for panel popup placement (ADR-0221) |
| `8cc89b85` | The start panel builds only the rows it can show |
| `2f266c1a` | QQ_Term is the desktop terminal; `qindaqt-terminal` removed (ADR-0222) |
| `3c79dd56` | Two pre-existing System Monitor `-Werror` breaks fixed |
| `f75dddd7` | Nine stock layouts, one per distinct feel (ADR-0223) |
| `72372645` | The three missing applets and the start-menu variants (ADR-0224) |

Cross-repository: `QindaQt_Apps` `b6df3d2` adds `qqterm -e PROGRAM ARG...`;
`QindaGentoo` `64c84a8` pins it as `gui-apps/qqterm-0.1.0_p20260920` from the
published bare repository instead of a working tree.

## Diagnoses that drove the work

- The reported "start menu opens in the upper-right corner of the start
  button" is QtWayland anchoring a `Popup.Window` at its parent item's
  top-right corner. `ControlPopupFrame` already worked around it and had never
  shared the workaround; ten popups had no placement at all.
- The reported start-menu sluggishness is measured, not inferred: a nested
  `Repeater` instantiated **336 of 336** program rows inside `popup.open()`,
  each resolving an icon through the theme. After: 16 of 336 over a 364 px
  viewport.
- `Terminal=true` desktop entries had **never** launched: the launcher's
  terminal command prefix was empty in production, so the executor returned its
  truthful refusal every time.
- Three stock profiles named plugins with no manifest
  (`application-launcher`, `grouped-task-list`, `centered-task-list`), so those
  controls silently resolved to nothing.

## Evidence

- Full build clean under the strict warning set.
- 193 focused rows green across profiles, applets, task list, start menu, panel
  QML, global menu, launcher, controls and session defaults.
- Broad safe suite green apart from `desktop.virtual.stage-closure`
  (`QindaQt.SettingsApp.StreamingBackend` has no `qmldir`), which reproduces on
  the clean tree and belongs to the audio/OBS lane.
- `mkdocs build --strict` and the link checker pass; 345 documents.
- Packaged as `gui-wm/qindaqt-desktop-0.1.0_pre20260920` pinning
  `72372645beb1211cf58485cd44ca6e87a8bbaef8`, with `gui-apps/qqterm` as a
  post-dependency (PDEPEND, not RDEPEND: qqterm build-depends on this package).

## Bounded caveats

- The gather overview requested during this wave is **not** delivered. The
  trigger path is prepared and KWin's top-left hot corner is released, but the
  surface needs live window previews, and the compositor texture readback for
  those is recorded as not yet implemented in
  [ADR-0119](../../../docs/wiki/adr/0119-authenticated-window-preview-channel.md)
  — the same blocker as dock hover thumbnails.
- `docs/wiki/handbook/catalog/assets.md` still records eleven profiles and
  older applet counts: it is an explicit snapshot at a named commit.
- `tools/check-source-shape` was not re-verified on the final tree; it exceeded
  a 300-second budget on this host. The change removes ~15k lines and adds no
  file near the decomposition threshold.
