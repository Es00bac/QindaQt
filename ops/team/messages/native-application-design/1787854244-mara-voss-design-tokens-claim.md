# Claim: QST-1 semantic design-token slice

- **Timestamp:** 2026-08-27T18:10:44Z
- **Worker:** Mara Voss — QindaQt Design Systems Engineer
- **Exact product base:** `dc29c88911f0ed6d381211027f16f46bbf92a07c`
- **Branch/worktree:** `worker/design-tokens-s1` at
  `/home/cabewse/work_SPaC3/container-wm-workers/design-tokens-s1`
- **Owning design:**
  [`1787853515-juno-park-design-handoff.md`](1787853515-juno-park-design-handoff.md),
  especially §§3–4 and 9–13.

## User-visible outcome

Implement QST-1 as a complete `src/design_tokens` module: immutable C++ token
values derived deterministically from public schema-v1 `ThemeSpec` values and
caller-supplied accessibility inputs, plus a GUI-thread, read-only
`QindaQt.Tokens 1.0` singleton. The slice preserves theme schema v1 and has no
Settings1, service, shell, application, or Kirigami dependency.

## Owned paths and coordination

Product edits are limited to new `src/design_tokens/**`,
`tests/design_tokens/**`, the owning design-token wiki page and ADR, and small
additive registry/documentation entries explicitly assigned by the manager.
Themes, profiles, Settings1, applications, shell, and platform-service paths
remain untouched. If the public ThemeSpec cannot support a total QST-1
derivation, I will post a separate boundary question rather than editing the
themes module.

Shared registry edits are coordination points. This claim notifies the native
application, themes/profiles, and build owners that I expect additive entries
in `src/CMakeLists.txt`, `tests/CMakeLists.txt`, `mkdocs.yml`, the module
boundary table, and directly affected roadmap/testing/theme documentation.

## Acceptance evidence planned

Focused Debug and Release tests will cover property/boundary derivation, the
documented WCAG pair scope for exactly five built-in themes, deterministic
accessibility transforms, offscreen singleton consumption/publication, and a
record-only non-flaky benchmark. Candidate gates will also include
`all_qmllint`, strict MkDocs, repository links, source shape, whitespace, and a
staged install/export/package consumer check. Only a passing exact milestone
commit will be offered for different-worker review.

