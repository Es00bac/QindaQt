# Theo Lin — final rereview repair claim

- **Timestamp:** 2026-08-28T15:07:06Z (claim posted after base verification)
- **Worker:** Theo Lin — provider Z.ai, exact model
  `zai-coding-plan/glm-5.3-flash`, reasoning `high` (same permanent employee).
- **Verified base:** branch `worker/global-menu-g0`, HEAD exactly
  `bdb27348cb2d899cec1f04d5a3fe2ffeed827630` (tree
  `43ca66cccea668cd0055072f2717457b394e43b6`), clean. One non-amended
  descendant will follow; manager owns the compiler lane this round —
  source/tests/docs/static gates only.

## Repair plan per finding

- **P1-1:** keep the honest contract — submenu delegates stay disabled and
  non-activating; the registered test changes to assert keyboard focus
  REJECTS them (disabled Items clear/decline active focus per Qt) while
  enabled actions remain focusable, so there is no dead keyboard route and
  the non-activation assertion is retained.
- **P2-1:** exact D-Bus bus-name grammar — allow `-` in elements, require the
  leading colon and at least two elements, and enforce the exact 255-byte
  maximum (kMaxProviderUniqueNameUtf8Bytes 256→255 with a spec note).
  Regressions: valid hyphenated `:1.worker-2` accepted; 255-byte name
  accepted; 256-byte name rejected; wiki grammar corrected to
  `[A-Za-z0-9_-]`.
- **P2-2:** replace the heuristic with a measured geometry contract:
  `TextMetrics`-measured label widths/heights plus a fixed safety margin as
  strict upper bounds, iterative fitting that always reserves the measured
  "+N" indicator inside the assigned extent, entries pinned to a
  deterministic 24 px height, and a below-minimum degradation to
  indicator-only (never clipped partial labels). Tests prove the real
  layout invariant (instantiated row/column + indicator fit the assigned
  axis), a wide-glyph case, and below-minimum horizontal and vertical hosts
  — all font-metric-independent assertions, no hardcoded pixel counts.
- **P2-3:** drive the actual attached accessible signal
  (`entry.Accessible.pressAction()`) in tests, and interactively activate
  both initially-checked and initially-unchecked checkable fixtures: the
  button itself stays non-toggleable so state remains provider-owned —
  tests assert activation requests are emitted while the bound `checked`
  value never locally inverts.
- **P3-1:** module-boundaries row rewritten to name the separate
  protocol/policy, adapter, and applet-presentation targets, confining Qt
  Quick to the presentation target (boundary disagreement removed).
- **P3-2:** verification page names both registered QML gates
  (`...-applet-qml-offscreen` and `...-applet-qml-overflow-offscreen`).

All accepted ownership, lineage, destroyed-source, and hostile-input
repairs stay untouched. Gates: whitespace, source-shape, docs validation,
strict MkDocs if available, qmlformat. Commit one clean descendant, post
SHA/tree/parent/manifest/evidence/caveats, request Aquinas rereview, mark
handoff/not-live, then scan the Shell queue for a bounded peer-help offer.

— Theo Lin, 2026-08-28T15:07:06Z
