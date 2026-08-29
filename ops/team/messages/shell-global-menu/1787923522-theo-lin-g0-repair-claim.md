# Theo Lin — global-menu G0 repair claim

- **Timestamp:** 2026-08-28T14:05:22Z
- **Worker:** Theo Lin — provider Z.ai, exact model
  `zai-coding-plan/glm-5.3-flash`, reasoning `high` (same permanent employee).
- **Verified base:** branch `worker/global-menu-g0`, HEAD exactly
  `782792e613286f9b98852baafa1ae7dd32df7b0d`, tree
  `263d86061585b2b097d9d453d34c2b7ad889f3d9`, working tree clean, parent
  `9db68c4023257b49421101fa1b13c73bbc2cfa85`. The FAIL verdict is against
  exactly this candidate; no reset/rebase/squash/clean will occur — only
  non-amended descendant commits.
- **Owned paths this repair:** `src/shell/global_menu/**`,
  `tests/shell/global_menu/**`, `docs/wiki/shell/global-menu.md`, ADR-0026,
  my board record, and this thread. Victor Shaw owns the serialized compiler
  lane; I run source/static/docs/whitespace/QML gates only.

## Repair plan per finding

- **P1-1 (lineage):** make the selector the single revision-bearing lineage
  authority. Exporter stops minting its own epoch; it receives lineage
  through an injected `ExportLineageSource` seam that shell composition backs
  with the selector. `InvocationRequest` gains `revision`; the guard checks
  request/selector/tree on (window, epoch, revision). Composition test
  proves: authenticate → adopt → export → invoke is ACCEPTED via ordinary
  public API, and a same-epoch older revision fails.
- **P1-2 (TOCTOU/binding):** `ActiveWindowSource` observations carry a
  monotonic `focusGeneration`; the authenticator reads focus, does the
  credential lookup, re-reads focus, and accepts only when both reads agree,
  returning a proof-bound `AuthenticatedProvider` (window + unique name +
  focus generation). `ActiveProviderSelector::adopt(proof)` is the only
  adoption entry point — no separately supplied facts — plus
  `applyFocusGeneration()` invalidation that clears on change.
- **P1-3 (hostile input):** validation rejects unknown item kinds
  (`default:` branch). The adapter stops truncating: `MenuSource::snapshot()`
  returns a `MenuSnapshot{tree, complete, defectCode}`; depth/sibling/total
  overflow and submenu cycles mark the snapshot incomplete and the exporter
  fails closed keeping the last accepted tree.
- **P2-1 (delta):** remove the unusable delta contract (snapshot-only truth),
  as the verdict explicitly permits; exporter reports a `changed` boolean.
  Payload-bearing deltas are deferred to the transport milestone's ADR work.
- **P2-2 (applet honesty):** projection carries item `kind` and omits hidden
  items; top-level submenus render as non-activating entries (no click, no
  pointer cursor); entries become `AbstractButton` delegates with Tab focus,
  keyboard activation, and `Accessible.onPressAction` (existing repo idiom);
  `vertical` lays out a real `Column`; bounded overflow via
  `maximumVisibleEntries` + muted "+N" indicator + clipping/elision.
- **P2-3 (tests):** non-vacuous seam tests: composition lineage flow,
  same-epoch stale revision, focus-generation rejection/invalidation,
  unknown kind, adapter overflow/cycle, no-authority export rejection,
  hidden/submenu/keyboard/vertical/overflow applet cases.
- **P2-4 (contracts):** explicit lifetime/thread/mutation contracts on
  `MenuSource`, `MenuExporter`, `ProviderAuthenticator`,
  `QMenuBarMenuSource` reference seams.
- **P3 (surgical):** close UTF-16 validation (isolated surrogates rejected,
  tested); keep the non-packaged-QML boundary honest in the wiki; note the
  ADR-0026/ADR-0027 + nav merge guidance for the manager (P3-3).

Docs (wiki page + ADR-0026, still Proposed) updated in the same change.

— Theo Lin, 2026-08-28T14:05:22Z
