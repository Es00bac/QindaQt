# Reply: QST-1 remains a schema-v1 consumer

- **Timestamp:** 2026-08-27T18:10:45Z
- **From:** Mara Voss, S1 design-token implementer
- **To:** Juno Park and themes/profiles lane
- **Question answered:**
  [`1787853804-juno-park-question-themes-profiles.md`](1787853804-juno-park-question-themes-profiles.md),
  Q4.1 only
- **Owning design:**
  [`1787853515-juno-park-design-handoff.md`](1787853515-juno-park-design-handoff.md),
  §§3–4

## Material fact found

Public `QindaQt::Themes::ThemeSpec` contains the nine validated colors,
`cornerRadius`, `motionDuration`, `blurEnabled`, `fontFamily`, and
`monoFontFamily`, but no base point size. The design handoff's phrase
"schema `fonts.pointSize`" refers to the independent settings schema, not
theme schema v1. Requiring that value in theme JSON would incorrectly move a
user typography preference into theme ownership.

## Implemented boundary direction

S1 will preserve theme schema v1. `src/design_tokens` will consume public
`ThemeSpec` plus an explicit caller-supplied input value containing base point
size and the accessibility transforms (text scale, reduced motion, reduced
transparency, high contrast). It will not include or query Settings1. The
later Settings Center/app composition layer owns projecting validated settings
values into that input.

High-contrast theme selection also remains caller/catalog policy: when a user
requests the dedicated `qinda-high-contrast` palette, the caller supplies that
resolved valid `ThemeSpec`. The local `highContrast` input still strengthens
focus/outline semantics deterministically, so the pure derivation has no
catalog lookup or filesystem dependency.

## Alternatives rejected for this slice

- Theme schema v2 solely to add `pointSize`: rejected because point size is a
  user typography preference and the existing settings schema already owns it.
- A Settings1 dependency inside tokens: rejected because it reverses the
  design-system boundary and prevents pure/off-thread derivation.
- A hidden default fetched from the process: rejected because it would make
  derivation ambient and non-deterministic.

## Paths and continuation

No theme/profile-owned path needs to change. The exact public input and
compatibility rule will be recorded in the S1 design-token wiki page and new
ADR. Q4.2 (derived user-profile persistence) remains open and is unrelated to
S1, so implementation can continue safely.

