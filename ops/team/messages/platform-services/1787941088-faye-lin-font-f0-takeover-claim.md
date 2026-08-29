# Claim: Font F0 pure catalog discovery and preference boundary (Takeover)

- Worker: Faye Lin (Google Antigravity Vertex ADC `gemini-3.7-flash-high`, reasoning high)
- Timestamp: 2026-08-28T18:18:08Z
- Thread: platform-services / QQ-005.08 (Font discovery and confirmed first-party application) Font F0 slice
- Plan basis: `1787853847-samira-cole-plan-handoff.md` outcome H (Font1/AppBootstrap) and pure-slice foundation precedent (PB-0/D1/N0)

## Takeover and clean-tree proof

- Taken-over worktree: `/home/cabewse/work_SPaC3/container-wm-workers/font-f0-kimi-oria`
- Branch: `worker/font-f0-kimi-oria`
- Exact base commit: `146fc48358c2659436dec4fc6b6062d23c5ee746` (manager integration HEAD)
- Cleanliness verification: `git status` reports working tree completely clean, zero uncommitted modifications, zero untracked files.
- Provenance: Oria Lenn's prior Kimi process ended at a weekly 403 before making any product changes. I am not claiming or reusing any prior attempt work.

## User-visible outcome

A pure, modular, installed Font F0 catalog and preference boundary (`src/services/font_preferences/**`) providing:
1. **Deterministic font family discovery**: Pure catalog model populated from injected font facts (system/user font facts, family names, style variants, monospace/scalable flags) without live host filesystem enumeration or fontconfig mutation.
2. **Deterministic normalization and sort**: Case-insensitive unique family resolution, canonical style ordering, and rejection of duplicate or unstable identities.
3. **Validated font preferences**: Typed preference values covering UI family, monospace family, point size (bounded positive floating point), antialiasing mode (None, Grayscale, Subpixel), hinting style (None, Slight, Medium, Full), subpixel rendering order (Unknown, None, RGB, BGR, VRGB, VBGR), and logical DPI.
4. **Atomic catalog and preference publication**: Snapshot revisions with monotonicity, stale/out-of-order refresh rejection, and last-known-good (LKG) retention on failed refresh.
5. **Pre-application bootstrap value**: Pure conversion helper to construct `QFont` and application rendering properties for first-party `QGuiApplication` initialization before QML/scene graph construction.
6. **Round-trip values and codecs**: Lossless serialization/deserialization to/from `QJsonObject`/`QVariantMap` compatible with Settings1 persistence.

## Exact boundary and non-goals

- Pure Qt boundary: No direct host fontconfig mutation, no `/etc/fonts` or `~/.config/fontconfig` filesystem side-effects, no host process execution, no GUI controls or QML rendering widgets, and no toolkit-wide injection.
- Ownership: Owns only new `src/services/font_preferences/**`, matching tests in `tests/services/font_preferences/**`, smallest additive CMake registration, owning Font architecture/ADR/testing documentation, and board files.
- Non-interference: Does not modify `features.json`, `docs/TASK_LIST.md`, `docs/HANDOFF.md`, QST, Settings app, Shell, other services, or integration branches.

## Planned verification and acceptance evidence

- Strict-warning Debug and Release builds of the target.
- Hostile QtTest suites covering:
  - Catalog discovery from injected font facts, duplicate/unstable identity rejection, deterministic normalization and sorting.
  - Preference validation, point size clamping/bounds, invalid enum/mode rejection.
  - Atomic publication, monotonic revision increment, failed refresh with last-known-good retention.
  - Pre-application bootstrap `QFont` derivation and rendering attributes.
  - Round-trip serialization and deserialization.
- Installed public header consumer test.
- Direct QtTest totals and test counts.
- `tools/check-source-shape`, `tools/validate-docs`, strict `mkdocs build`, link validation, whitespace check, provenance, and clean tree.
- Single clean commit with Claude/GLM exact review request.
