# Icon choice

- Status: idle — icon selection and native fixture repair accepted for packaging; available for delivery regressions.
- Base: 2a64dd0a
- Branch: fix/icon-theme-choice
- Owned paths: Themes icon catalog, appearance settings/schema/UI, AppAppearance/native Qt bridge, shell icon runtime/composition, focused tests/docs. File Manager paths belong to manager.

## Updates

- 2026-09-27T19:05:00Z: Claimed isolated icon-choice outcome. Follow theme remains default; choices come from bounded installed indexes. Preserve task row/delegate identities with provider cache replacement and a QML icon revision, not shell reload.
- 2026-09-27T19:15:03Z: Candidates930f68fc,88e0ef76,8c0bc557 pushed. Manager native first-pass catalog/shell icons/controller/model/page gates passed; obsolete key-count assertion repaired in8c0bc557. Manager explicitly expanded ownership to File Manager provider URL/main wiring; palette Main.qml remains untouched.
- 2026-09-27T19:15:03Z: Local exact-source catalog3/3 and File Manager provider9/9 pass, including unchanged existing image object with new URL and changed red/blue pixels. Manager rebuilding final integrated candidate; reviewer awaiting those gates. Documentation419pages and diff-check pass.
- 2026-09-27T19:17:00Z: Final product8c0bc557 handed off after localcatalog3/3 and fileprovider9/9; manager owns finalnative gate confirmation and both-host overlay installation. Read shell queue and offered exact regression support rather than claiming unrelated work.
- 2026-09-27T19:18:00Z: Manager expanded native platform gates exposed fake Settings1 omitting the additive icon key, producing a null D-Bus variant. Repairing fixture to supply empty Follow theme and include icon-key invalidation.
- 2026-09-27T19:21:00Z: Manager confirms final native appearance12/12 and expanded platform8/8 after fixture4071579d. Production candidate remains8c0bc557; f6cb7107 is an additional test-only fallback assertion, not a packaging prerequisite. No further scope changes; delivery belongs to manager.
