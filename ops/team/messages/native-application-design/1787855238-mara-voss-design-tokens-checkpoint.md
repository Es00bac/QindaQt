# Checkpoint: QST-1 implementation and initial focused proof

- **Timestamp:** 2026-08-27T18:27:18Z
- **Worker:** Mara Voss, S1 design-token implementer
- **Base:** `dc29c88911f0ed6d381211027f16f46bbf92a07c`
- **Owning design:**
  [`1787853515-juno-park-design-handoff.md`](1787853515-juno-park-design-handoff.md)
- **Boundary reply:**
  [`1787854245-mara-voss-qst1-theme-boundary-reply.md`](1787854245-mara-voss-qst1-theme-boundary-reply.md)

## Material state

The isolated product branch now has the complete uncommitted S1 implementation:
a pure `QindaQt::DesignTokens` value/deriver target and separate
`QindaQt.Tokens 1.0` QML adapter. The public ThemeSpec remained unchanged;
base point size and accessibility preferences are explicit normalized caller
inputs. QML receives read-only aggregate maps and cannot publish or select a
theme. The facade rejects wrong-thread/null/invalid publication without losing
the last complete generation.

The candidate also adds accepted ADR-0013 (QST-1 ownership; adapter-only future
Kirigami reuse), the normative design-token page, and additive boundary,
roadmap, testing, theme-reference, ADR-index, and MkDocs navigation updates.
No theme/profile/Settings1/shell/application path was edited.

## Evidence so far

- Focused Debug build: succeeded with strict warnings.
- `ctest -R '^qindaqt\.design-tokens-'`: **4/4 passed**. This covers
  property/boundary derivation, exact five-built-in WCAG pairs, deterministic
  accessibility transforms, offscreen QML singleton consumption/publication,
  and the record-only benchmark.
- `all_qmllint`: passed (`Nothing to do` for the C++-only QML module).
- `tools/check-source-shape`: 722 files checked, zero violations; largest new
  production file is 240 physical lines.
- `tools/validate-docs`: 42 Markdown documents/navigation entries passed.
- `uvx --from mkdocs mkdocs build --strict`: passed.

## Remaining gates

Release focused build/tests, repeated benchmark recording, staged install and
installed QML import/header/library inspection, full appropriate Debug/Release
suites, whitespace/source-shape/docs rerun, then one exact milestone commit and
different-worker review. No completion claim is made yet.

