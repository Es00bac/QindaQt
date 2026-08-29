# Appearance Settings S0 repaired-candidate exact verdict

- Timestamp: 2026-08-28T10:10:05-06:00
- From: Maxwell the 2nd
- To: Turing the 2nd, Katherine Cho, Program Manager
- State: exact rereview complete; **PASS**
- Exact candidate: `d71fac4a2c7e8944822b3185aee5bb43acd455c7`
- Exact tree: `3e447c15a7a7b42634dc4d0e7bf955e4bc502747`
- Parent: rejected `9a495aad63034a5fa02613df7ab0d17b9d920385`
- Finding count: **P0/P1/P2/P3 = 0/0/0/1**
- Requested next action: integrate this exact commit and rerun the affected
  gates on the combined manager tree

## Verdict

**PASS.** Candidate `d71fac4` repairs every finding in Maxwell's preserved
`0/7/4/2` verdict without replacing the accepted Appearance architecture. No
P0, P1, or P2 defect remains in the exact candidate. The detached review tree
is clean and contains no reviewer product edit.

## Former finding closure

1. `Main.qml` now gives each route an optional root property and instantiates
   only the active route's required model; the full-root construction gate
   launches both routes with only that model.
2. Build-tree imports are admitted only for an executable physically below the
   configured build root. Installed imports resolve from the executable's own
   prefix, and the executable/Appearance/Controls/Tokens binaries carry only
   relative loader paths.
3. Font-family and wallpaper `onTextEdited` handlers read the control's real
   `text`; ordinary keyboard entry reaches exact draft keys and strings.
4. Revert from an answerable Conflict clears per-key intent, diagnostics, save
   results, and Conflict state before returning to clean Ready.
5. Per-key dirty intent rebases every untouched field to clean same-owner or
   replacement authority while preserving only genuine user edits; hostile
   evidence verifies the next outbound write contains exactly the edited key.
6. The compact form has a visible scrollbar, generic focus reveal, forward and
   reverse traversal, and bounded scroll commands; the compiled 420x320 page
   gate keeps every required editor/action focus target visible.
7. One bounded result entry per captured key distinguishes Applied, Failed,
   Conflict, Uncertain, and Not attempted; a later-key persistence failure
   visibly retains the earlier Applied key and later Not-attempted key.
8. Theme directories merge in precedence order, retain unique built-ins, fail
   malformed input closed, and prove the exact five shipped IDs including
   `qinda-macos`.
9. The three additive v2 keys have exact shipped-schema type/default/constraint
   coverage, while v1 migration leaves them absent from the migrated layer and
   resolves their active system defaults without manufacturing overrides.
10. Empty schema-nonempty Theme and Font Family values fail complete snapshot
    decoding, alongside wrong types, unknown tokens, non-finite/out-of-bound
    numbers, and missing keys.
11. The Appearance module's ownership, allowed dependencies, and prohibited
    platform/persistence/transport reach are normative in module boundaries.
12. The public constructor states client/facade lifetime and GUI-thread rules;
    construction asserts thread agreement and guards facade destruction with
    `QPointer`.
13. Every enum-like draft field requires an exact `QString` metatype before
    token decoding; adversarial integer/Boolean conversions are rejected for
    all four fields without dirtying the draft.

## Independent executable evidence

Fresh dependency-light Debug configuration used `BUILD_TESTING=ON`, shared
loadable modules, strict warnings, host-uinput tests disabled, and compositor,
shell, and production-shell targets disabled. The preserved serial build then
completed these targets with exit 0:

```text
qindaqt_appearance_values_tests
qindaqt_appearance_preview_tests
qindaqt_appearance_model_tests
qindaqt_appearance_page_tests
qindaqt-settings
qindaqt_settings_migration_tests
```

Direct QtTest, all exit 0:

- Appearance values: 7 passed, 0 failed.
- Appearance preview/catalog: 8 passed, 0 failed.
- Appearance ordinary model: 11 passed, 0 failed.
- Appearance adversarial model: 6 passed, 0 failed.
- Appearance page under `QT_QPA_PLATFORM=offscreen`, software rendering, and
  forced accessibility: 9 passed, 0 failed.
- Settings migration: 10 passed, 0 failed.

Registered CTest, all exit 0:

- `^qindaqt\.appearance-(values|preview|settings-model|page)$`: 4/4.
- `^qindaqt\.settings-app-(offscreen|rejects-unknown-route|desktop-identity|route-construction|installed-routes)$`:
  5/5. The final row installs only `SettingsAppearanceRuntime` into a clean
  build-local prefix, unsets display/Wayland/QML/library/user-config authority,
  discovers its own themes, and keeps both routes constructed until the
  harness's bounded timeout.
- `^qindaqt\.settings-migration$`: 1/1.

The installed stage contains the executable and desktop file, all five theme
JSON files, and complete Appearance/Controls/Tokens QML modules. Independent
ELF inspection finds these loader paths and no absolute build path in any
`DT_RUNPATH`:

- executable: `$ORIGIN/../lib/qt6/qml/QindaQt/Tokens`;
- Tokens backing library: `$ORIGIN:$ORIGIN/../lib`;
- Controls backing library: `$ORIGIN/../Tokens`;
- Appearance backing library: `$ORIGIN/../../Tokens:$ORIGIN/../../Controls`.

## Independent static/documentation evidence

- `python3 tools/check-source-shape`: PASS across 1,033 files; one disclosed
  nonblocking decomposition-review warning remains for the 563-nonblank-line
  ordinary model test fixture. New hostile behavior and production draft/
  result responsibilities are already split into cohesive files.
- `python3 tools/validate-docs`: PASS, 65 documents and navigation valid.
- `uvx --from mkdocs mkdocs build --strict`: PASS.
- Exact `git diff --check`, detached cleanliness, commit/tree/parent identity,
  temporary-marker scan, and source/docs review: PASS.

## Nonblocking P3 and later boundaries

The page traversal fixture instantiates one theme-card delegate and does not
directly press Page Up/Page Down or Ctrl+Home/Ctrl+End. It does execute the
complete forward/reverse editor/action chain at 420x320 and the implementation's
focus reveal is delegate-generic; I found no product defect. Expand that one
fixture when the broader multi-theme/AT-SPI matrix lands.

No live session bus, persisted restart, compositor-applied wallpaper/font/
scale, live AT-SPI, private nested desktop, physical hardware, or screenshot
gate is claimed by this S0 verdict. Those remain the explicitly documented
later integration boundaries, not blockers for this candidate.

The compiler/runtime lane is released. Maxwell remains available for exact
combined-tree questions; no repair rereview is required for this commit.
