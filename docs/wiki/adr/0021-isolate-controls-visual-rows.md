# ADR-0021: Isolate every Controls visual row in its own process

- **Status:** Accepted, amended 2026-09-02, amended 2026-09-03
- **Date:** 2026-08-27
- **Owners:** Controls and testing working groups
- **Supersedes:** None
- **Superseded by:** None

## Context

The Controls visual matrix renders 25 exact theme, scale, and width-profile
combinations through Qt Quick's offscreen software backend. A combined QtTest
process made row identity and renderer/window lifetime implicit while captured
Dusk and Qinda macOS compact pixels could retain a one-character text clip even
though the live QML object graph reported correct card, content, text, and
action geometry. Reusing one view also allowed animated resize state to cross
rows. Process isolation alone did not settle pixels: a frame diagnostic proved
that capture also had to follow the fixture's published QST motion boundary.
The two requirements address separate concerns—one exact row lifetime and one
semantically derived capture boundary.

The owning test contract is [Development and testing harness](../development/testing-harness.md#current-reusable-controls-proof).

## Decision

Each of the 25 Controls visual combinations runs as one named CTest and one
fresh process. Every CTest invokes the same compiled visual-test executable
with exactly one validated QtTest data selector. The wrapper rejects a missing
or scale-incompatible selector, executes no other data row, and verifies that
the requested row is the sole tagged visual pass.

The stable focused prefix remains `qindaqt.controls-`. Visual test names encode
scale, theme, and width profile. Baseline generation and normal comparison use
the same process boundary, environment, DPR and pixel-size assertions, source
paths, and reviewed PNG locations. Each row waits through the fixture control's
published transition duration before requesting the frames used for capture;
reduced-motion duration comes from QST rather than a test override.

## Consequences

- Captures cannot inherit renderer, font-node, window-size, or control state
  from another theme/profile row.
- Failures identify the exact theme, scale, and width profile at the CTest
  boundary rather than only inside combined output.
- The Controls selector discovers 29 tests: behavior, 25 visual rows, source
  policy, staged installed import, and PSS measurement.
- Visual execution pays process startup once per row. That bounded cost is
  accepted for deterministic review evidence and remains serial by default.
- The source-policy gate must prove rejection of missing and unknown selectors;
  every registered visual row must prove exactly one matching QtTest pass.
- Combining rows in one visual process or reusing a view across rows violates
  this decision even when object-property assertions pass.

## Revisit when

Reconsider only when a replacement runner proves the same exact row identity,
lifetime isolation, QST-derived motion boundary, DPR/pixel assertions,
generate-versus-compare parity, and deterministic reviewed baselines.

## Amended (2026-09-02): fonts are byte-pinned fixtures

**Context.** The original decision left glyph sources to "deterministic
environment substitution": the fixture substituted the schema's `Inter` and
`JetBrains Mono` names with whatever `Noto Sans` and `Noto Sans Mono` the host
image had installed. The host Noto package updated on 2026-09-02 after the
baselines were captured on 2026-08-27, and every visual row failed with
glyph-rendering drift while the layout stayed identical. Environment
substitution therefore did not keep the gate deterministic across host font
updates.

**Amendment.** The visual fixture now vendors the exact font files it renders
into `tests/controls/fonts/` (Noto Sans Regular/SemiBold/Bold and Noto Sans
Mono Regular, SIL Open Font License 1.1). Their name records are rewritten to
the repository-owned families `QindaQt Sans` and `QindaQt Sans Mono`; the
glyph data stays byte-identical to upstream. The fixture registers the files
through `QFontDatabase::addApplicationFont`, requires every registration to
expose exactly the expected family, and substitutes the theme schema's
`Inter` and `JetBrains Mono` names to those repository-owned families.
Because no host font can declare the renamed families, the fixture cannot
collide with or be shadowed by host-installed Noto regardless of Qt's
match order, so a host Noto package update can no longer change the rendered
bytes. Registration failure is fatal. The `qindaqt.controls-font-pinning`
row proves the substituted family resolves to the vendored bytes, and three
`qindaqt.controls-font-fixture-*` rows prove the fail-closed behavior.

The row environment deliberately keeps the documented host fontconfig
configuration. An empty or missing fontconfig configuration is not a font
byte pin: measured on 2026-09-02 it changes glyph advances (the gallery
header description re-wraps from two lines to one) and removes the host
fallback face that supplies the checked ThemeCard glyph, so baselines
captured under it would embed a different rasterization environment instead
of pinning bytes. Process isolation, the QST motion boundary, DPR/pixel
assertions, and reviewed baselines are unchanged; this amendment only changes
where glyph bytes come from.

**Consequences.**

- A host font package update can no longer fail or silently alter the gate;
  only a reviewed change to the vendored fixture can.
- Renewing the vendored fonts is a deliberate baseline regeneration with
  review of the exact glyph diff, not an ambient host event.
- The substitution of `Inter`/`JetBrains Mono` names to the registered
  repository-owned families remains, so the fixture still exercises the theme
  schema's font fields.
- The gate still depends on the documented host fontconfig configuration for
  rasterization parameters and Latin-supplement fallback glyphs; a host
  fontconfig configuration change or a removed DejaVu package therefore still
  requires baseline review, and is visible as baseline drift rather than as a
  silent pass.

## Amended (2026-09-03): complete catalog substitution, valid checksums, and a host-Noto-hidden canary

**Context.** Independent review of the 2026-09-02 amendment (candidate
`bf1c83a`) rejected its host-independence claim. `data/themes/qinda-high-contrast.json`
names `Noto Sans`/`Noto Sans Mono` directly, and the fixture substituted only
`Inter`/`JetBrains Mono`, so the five high-contrast rows still rendered the
host Noto family: under a host-Noto-hidden fontconfig they drifted by 145,270
pixels while the four Inter-based themes passed. The review also found that
the checked-in renamed fonts carried invalid sfnt checksums (the renewal tool
never recomputed the `name` table directory checksum or
`head.checkSumAdjustment`) and that the staged installed-consumer row ran an
unscoped whole-tree install that a focused Controls build could not satisfy.

**Amendment.** Three repairs, all inside the test fixture:

1. `pinDeterministicFonts()` now rewrites every theme in the product catalog
   with its `fontFamily`/`monoFontFamily` replaced by the registered
   repository-owned families, into pinned runtime theme copies that
   `publishTheme()` loads (the product theme files are untouched), and keeps
   `QFont` substitutions for the original catalog names. The rewrite is
   necessary, not stylistic: `QFont::insertSubstitution` is consulted only
   when the requested family is absent from the host, so with host Noto
   installed a substitution for `Noto Sans` is ignored (verified against
   Qt 6.11). Deriving the rewrite from the catalog at runtime means a future
   theme that names another host family is covered automatically.
2. The renewal tool (`tests/controls/fonts/rename_family_names.py`) recomputes
   every table directory checksum and `head.checkSumAdjustment` per the
   OpenType specification, and the four vendored files were regenerated from
   the upstream host builds with valid checksums (all non-`name` tables,
   including glyph data, remain byte-identical; `head` differs only in the
   adjustment field). The `qindaqt.controls-font-pinning` row now validates
   those checksums and fails on a malformed fixture.
3. A dedicated canary row,
   `qindaqt.controls-visual-no-noto-100-qinda-high-contrast-compact`, reruns
   the previously bypassing row under the checked-in
   `tests/controls/fontconfig/no-noto/fonts.conf` (documented host
   configuration minus exactly the families the catalog can name), applied
   through `FONTCONFIG_FILE`. A `FONTCONFIG_FILE=/dev/null` run is explicitly
   not evidence: fontconfig cannot parse it and silently falls back to the
   standard host configuration. The staged installed-consumer row installs
   only the `ControlsQmlModules` component declared in
   `tests/controls/CMakeLists.txt`, so the complete selector is runnable
   after the focused Controls target build.

**Consequences.**

- Every theme row, including Qinda High Contrast, renders only vendored
  bytes; the regenerated baselines were byte-identical to the reviewed set
  because the vendored glyph tables match the current host Noto build.
- A fixture regression in either direction fails a named row: rendering host
  Noto fails the canary, malformed vendored files fail the pinning row, and
  missing/corrupt/renamed fixtures abort fail-closed as before.
- Renewing the vendored fonts now requires checksum-valid output; the
  renewal tool and the pinning row keep each other honest.
- The host fontconfig dependency (rasterization parameters and the DejaVu
  fallback glyph) is unchanged from the 2026-09-02 amendment; the canary
  proves independence from the Noto families specifically, not from the
  whole font stack.
