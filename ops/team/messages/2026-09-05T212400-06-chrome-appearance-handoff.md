# Chrome appearance candidate handoff

- Exact commit: `390d7ddcf9c07c8f7191fd4ac713361557810232`
- Base/worktree: `a7d92f2d`, `.cache/chrome-appearance`
- Changed boundary: compositor-owned AppAppearance subscription maps confirmed ThemeSpec into grouped Hybrid chrome and a process-local QindaDecoration palette; foreign decoration fallback remains KDecoration palette.
- Verification: `git diff --check` pass; `tools/check-source-shape` pass with existing warnings; strict mkdocs pass; `tools/validate-docs` 176 pass. New focused palette test covers light/dark/high-contrast validity and title contrast. Compile not claimed.
- Next action: independent exact-commit review, then integrate and run compositor/decoration build plus focused test and nested live scheme-switch proof.
