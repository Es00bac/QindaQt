# Blocking finding: reduced transparency is not total for schema-v1 colors

- **Timestamp:** 2026-08-27T18:58:28Z
- **Reviewer:** Iris Quill
- **Exact candidate:** `73dd763e52c132cd5c7f629e697fb93a92392b3a`
- **Severity:** P1 accessibility/compatibility contract defect
- **Verdict impact:** independently blocks acceptance

## Reproduction

Theme schema v1 accepts every valid Qt color and does not require opaque alpha.
I loaded (through the public `ThemeLoader::fromJson`, not direct ThemeSpec
construction) a complete schema-v1 theme whose canvas/surface/surfaceRaised are
`#80ffffff`, then derived QST-1 with `reducedTransparency=true`.

The review-only ignored-build probe records:

```text
bgAlpha=128 disabledAlpha=192 hoverAlpha=138
```

Evidence and source live only under the detached review worktree's ignored
build directory:

- `build/review-design-tokens-debug/adversarial_alpha_probe.cpp`
- `build/review-design-tokens-debug/adversarial_alpha_probe`
- `build/review-design-tokens-debug/adversarial-alpha-probe.log`

The product checkout remains clean.

## Contract conflict

- `docs/wiki/architecture/design-tokens.md` says reduced transparency
  composites alpha roles to opaque surface colors and that the resulting roles
  are deterministic opaque colors.
- `src/design_tokens/src/token_deriver.cpp:25-31` composites each overlay over
  `surface` unchanged. If the loader-valid surface has alpha below 1, standard
  source-over composition remains translucent.
- `src/design_tokens/src/token_deriver.cpp:148-150` also publishes the raw
  translucent background roles unchanged under the reduced-transparency input.
- Existing tests cover only the five opaque built-ins, so the claimed total
  schema-v1/accessibility behavior is not exercised.

This is not a hypothetical malformed direct value: `ThemeLoader` accepted the
theme under the public schema-v1 rules. S1 explicitly promises compatibility
for future/user schema-v1 themes and deterministic accessibility transforms.

## Required repair

Define and implement a total opaque-flattening policy for reduced transparency
when source theme colors carry alpha, without silently narrowing accepted theme
schema v1. The policy must cover background roles and every alpha-derived role,
then add loader-valid translucent-theme tests asserting alpha 255 and stable
values. Update the normative derivation wording if the exact flattening base or
semantics change. Preserve the five built-in contrast gates and all existing
publication/package behavior.

The repaired exact candidate should be re-reviewed independently together with
the installed C++ consumer repair already posted in this thread.
