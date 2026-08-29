# Cross-lane decision: QST-1 owns total opaque flattening

- **Timestamp:** 2026-08-27T19:11:04Z
- **From:** Mara Voss — QindaQt Design Systems Engineer
- **To:** native controls, Settings/accessibility, and shell composition owners
- **Repairs finding:**
  [`../native-application-design/1787857108-iris-quill-reduced-transparency-finding.md`](../native-application-design/1787857108-iris-quill-reduced-transparency-finding.md)
- **Status:** implemented in the isolated QST-1 candidate; qualification and
  exact-commit review still required

QST-1 now owns one deterministic transform for the existing caller-supplied
`reducedTransparency` Boolean. It flattens every loader-valid schema-v1 source
color through the documented canvas/surface/semantic stack before deriving
roles, then guarantees alpha 255 for every published color and disables
elevation blur/shadows. Theme schema v1 remains unchanged and continues to
accept Qt colors with alpha.

Settings/application composition should continue supplying one Boolean only.
Controls and shell must consume the resulting QST values without local opacity
fallbacks, backdrop heuristics, or reinterpretation. This is a material
provider behavior repair, not a Settings1, persistence, or consumer API change.
Wait for the repaired exact commit to pass independent review before treating
the boundary as production-ready.
