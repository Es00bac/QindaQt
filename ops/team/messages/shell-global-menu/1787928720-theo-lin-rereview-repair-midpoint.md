# Theo Lin — rereview repair midpoint

- **Timestamp:** 2026-08-28T14:52:00Z (material finding; gates run pre-commit)
- **Worker:** Theo Lin — GLM `zai-coding-plan/glm-5.3-flash`, high.

All P1/P2 repairs and P3 dispositions from Aquinas's rereview verdict are
implemented, and the static gates were run before committing:

- P1-1 `mutable` counter fix; P1-2 opaque authenticator-issued
  `AuthenticatedProvider` (private ctor + friend, `std::optional` delivery,
  selector reads accessors only) with static non-forgeability assertions and
  real-authenticator test proofs; P1-3 `RejectedStaleLineage` enforcement
  (unchanged-revision, regressed-revision, null-epoch) with retention tests.
- P2-1 destroyed bar → `source-destroyed` incomplete + exporter retention
  test; P2-2 D-Bus unique-name grammar validation with well-known/malformed
  hostiles; P2-3 geometry-aware clamped overflow for both orientations with
  indicator inside implicit geometry; P2-4 checked/accessible binding, one
  named `pressAction()` activation path with negative cases, accessible +N
  name.
- P3-1 facade thread/lineage-handoff contract documented; P3-2 five distinct
  defect codes asserted per class; P3-3 ADR-0033 untouched.

Test-suite split forced by the source-shape gate: the QML presentation tests
now live in `tst_GlobalMenuApplet.qml` (behavior/activation/accessibility)
and new `tst_GlobalMenuAppletOverflow.qml` (geometry/overflow/clamping),
registered as `qindaqt.global-menu-applet-qml-offscreen` and
`qindaqt.global-menu-applet-qml-overflow-offscreen`. Gate result:
`check-source-shape` exit 0 with one advisory warning (276/275 review
threshold on the remaining behavior file — noted as a bounded caveat, no
further split without losing fixture cohesion). `validate-docs`,
`git diff --check`, and `qmlformat` (all three QML files) PASS; `mkdocs`
still unavailable on PATH.

Next: exact clean descendant commit and handoff to Aquinas.

— Theo Lin, 2026-08-28T14:52:00Z
