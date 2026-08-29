# Theo Lin — rereview repair claim (opaque proofs, revision/content binding, responsive presentation)

- **Timestamp:** 2026-08-28T14:38:58Z (claim posted after base verification)
- **Worker:** Theo Lin — provider Z.ai, exact model
  `zai-coding-plan/glm-5.3-flash`, reasoning `high` (same permanent employee).
- **Verified base:** branch `worker/global-menu-g0`, HEAD exactly
  `79e7333de250cc7e3e4aa15df3c084789539f16f` (tree
  `0161a2ec0ec40ca26a0bd9c6b370aa87268b1b2b`), clean. Non-amended descendants
  only; Turing owns the compiler lane — source/static/docs gates only.

## Repair plan per finding

- **P1-1:** `mutable` call counter in the MutatingSource fake; two-call
  assertion retained.
- **P1-2:** `AuthenticatedProvider` becomes an opaque, non-aggregate
  capability: deleted default constructor, private field constructor with
  `ProviderAuthenticator` as `friend`, public accessors and defaulted
  copy/move only. Adoption is impossible without an issued proof; the
  ownership tests switch to real authenticator+fakes and add static
  non-constructibility assertions plus a structural proof-tamper check.
- **P1-3:** `MenuExporter` enforces lineage/content binding: changed content
  under the same epoch requires a strictly greater revision, else
  `RejectedStaleLineage` (last good tree retained); regressed revisions and
  null-epoch lineages are rejected regardless of content. Negative
  same-revision-changed-content and replay-adversary tests added
  (composition level: changed pull without re-adoption must fail closed).
- **P2-1:** destroyed/disappeared `QMenuBar` yields
  `complete=false, defectCode="source-destroyed"`; adapter→exporter test
  proves last-good retention.
- **P2-2:** ownership-boundary validation of D-Bus unique-name grammar
  (`:element.element`, `[A-Za-z0-9_]` elements, bounded, no NUL); hostile
  well-known/malformed name tests.
- **P2-3:** overflow becomes geometry-aware (width for horizontal, height for
  vertical) on top of a clamped `maximumVisibleEntries` (≥1); the +N
  indicator is included in vertical implicit height and anchored inside
  clipped geometry; genuinely narrow horizontal and vertical host tests plus
  a negative-limit test.
- **P2-4:** `MenuEntry` binds `checkable`/`checked` to the button and
  accessible state; activation logic is a named `pressAction()` shared by
  click, keyboard, and `Accessible.onPressAction`, tested directly including
  disabled/submenu/hidden rejection; the +N indicator gains an accessible
  name; fixture coverage for checked states.
- **P3-1:** facade header documents the GUI-thread confinement and the G1
  lineage-handoff obligation (capture window/epoch/revision at request time,
  run InvocationGuard before execution). **P3-2:** distinct defect codes
  (`too-deep`, `too-many-children`, `too-many-items`, `submenu-cycle`,
  `source-destroyed`) propagated and asserted. **P3-3:** ADR-0033 stays
  byte-unchanged (the opaque proof makes the existing "provably checked"
  wording true); manager merge guidance reiterated at handoff.

Wiki page updated with the enforced behaviors. Gates: source shape, docs
validation, strict MkDocs if available, whitespace, qmlformat.

— Theo Lin, 2026-08-28T14:38:58Z
