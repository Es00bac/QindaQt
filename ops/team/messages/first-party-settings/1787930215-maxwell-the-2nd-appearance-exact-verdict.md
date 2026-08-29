# Appearance Settings S0 exact-candidate verdict

- Timestamp: 2026-08-28T09:16:55-06:00
- From: Maxwell the 2nd
- State: exact review complete; **FAIL**
- Exact candidate: `9a495aad63034a5fa02613df7ab0d17b9d920385`
- Exact tree: `84c991cf91f2b74cd138b74eb8f9cfdba63f8e15`
- Public comparison base: `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- Finding count: **P0/P1/P2/P3 = 0/7/4/2**

## Blocking P1 findings

1. The Appearance executable branch supplies `appearanceSettings` and `route`
   only, but root `Main.qml` still requires `quietingSettings`. Root creation
   fails before the inactive route Loader can help
   (`src/apps/settings_center/main.cpp:128-136`, `Main.qml:8`).
2. The executable directly needs `libqindaqt_tokens_qml.so`, but its installed
   RUNPATH does not include that module's install directory. The added QML
   import path also matches neither the observed build module root nor the
   installed `lib/qt6/qml` root. This is a build/install runtime layout failure,
   not merely missing test evidence (`src/apps/settings/appearance/CMakeLists.txt:25-31`,
   `src/design_tokens/CMakeLists.txt:68-83`, `main.cpp:72-78`).
3. Both editable text fields bind a parameter to zero-argument
   `TextInput::textEdited()`, then forward the undefined parameter. Strict draft
   typing rejects it, so typing a font family or wallpaper does not update the
   model (`AppearanceFontSection.qml:51`, `AppearanceDesktopSection.qml:53`,
   `appearance_settings_model.cpp:162-187`).
4. Revert in a fresh answerable Conflict copies confirmed values but neither
   clears conflict intent nor returns to Ready. The Revert control hides after
   cleaning the draft while stale Conflict remains
   (`appearance_settings_model.cpp:206-213,324-331`).
5. Later authoritative snapshots never rebase clean/untouched draft fields.
   This creates phantom dirty state and, when one field was genuinely edited,
   queues stale unrelated fields for overwrite
   (`appearance_settings_model.cpp:423-450`).
6. The long form is hosted by a raw clipped `Flickable` with no key scrolling,
   scrollbar, or focused-control reveal. Keyboard focus can move outside the
   default/minimum viewport; the claimed traversal test presses Tab only once
   and never checks visibility/order/reverse traversal
   (`AppearancePage.qml:82-114`, `tst_appearance_page.cpp:430-458`).
7. The deliberately non-atomic transaction sequence exposes only a global
   status/error. If an earlier key is Applied and a later key fails, persisted
   partial success is invisible, contradicting ADR-0028's promised truthful
   per-key result presentation
   (`appearance_settings_model.h:34-51`,
   `appearance_settings_model.cpp:337-383`).

Each P1 has a preceding board message containing the deterministic reproduction
and bounded repair/test requirement.

## P2 findings

1. The route stops at the first loadable theme directory, and
   `ThemeCatalog::loadDirectory()` replaces rather than merges its catalog.
   One user theme can therefore hide every built-in theme and invalidate the
   default (`main.cpp:91-105`, `src/themes/src/theme_catalog.cpp:36-67`). The
   built-in preview helper also silently omits failed theme files and never
   asserts all five required IDs, including `qinda-macos`
   (`tst_appearance_preview.cpp:14-29,94-119`).
2. Three new schema keys lack exact built-in-schema default/constraint and
   migration coverage. The test named `scopedKeysMatchSchemaKeys` does not read
   the schema at all; it checks a hand-written list against itself
   (`data/settings/schema-v2.json:12-36`,
   `tst_appearance_values.cpp:152-162`).
3. Snapshot decoding accepts empty Theme and Font Family even though schema-v2
   marks both non-empty. The model reaches ready authority with an invalid
   baseline rather than failing closed; hostile decode coverage uses only a
   wrong QVariant type (`appearance_values.cpp:19-22,147-168,281-297`).
4. The new `src/apps/settings/appearance` public/module boundary is absent from
   normative `docs/wiki/architecture/module-boundaries.md`, leaving its allowed
   dependencies and prohibited persistence/platform/QML reach unspecified
   despite the repository's documentation contract.

## P3 findings

1. The public model constructor stores a client reference and raw optional
   facade pointer without stating the required lifetime and thread confinement
   of either dependency (`appearance_settings_model.h:53-63,123-126`).
2. `setDraftValue()` advertises strict field typing but enum-token branches call
   `QVariant::toString()` without first requiring `QString`, unlike direct string
   fields (`appearance_settings_model.cpp:145-190`). Add wrong-metatype tests and
   reject conversion for every enum field.

## Positive evidence retained

The candidate has substantial valid structure: typed domain values, complete
preview maps for successfully loaded themes, bounded token publication,
owner/epoch replacement abort during commit continuation, ordinary card/toggle
wiring, modular QML section files, desktop identity set before UI construction,
registry/install declarations, ADR-0028, and updated owning documentation.

Independently rerun source-safe gates on the exact detached candidate:

- `git diff --check 9db68c4..9a495aa`: PASS; detached product status clean.
- `python3 tools/check-source-shape`: PASS across 1,024 sources; one disclosed
  555-nonblank-line model-test decomposition-review warning.
- `python3 tools/validate-docs`: PASS, 65 documents.
- `uvx --from mkdocs mkdocs build --strict`: PASS.

Per the manager's serialized-lane instruction I did **not** compile, run CTest,
launch a GUI/session, inject input, alter configuration, or touch host state.
Turing's claimed build/CTest results are preserved as implementer evidence, not
misrepresented as independently rerun review evidence.

## Required next action

Return these exact findings to Turing the 2nd in the preserved implementation
worktree, keep the reviewer available, and rereview one repaired immutable
commit. Minimum acceptance must include an exact full-root Appearance launch,
sanitized staged/installed route launches, text-entry interaction, conflict
Revert, clean and partially dirty external rebase cases with exact outbound key
assertions, later-key partial failure presentation, and full visible forward/
reverse keyboard traversal, in addition to the existing focused gates.

The product review worktree was kept read-only and remains at the exact
candidate.
