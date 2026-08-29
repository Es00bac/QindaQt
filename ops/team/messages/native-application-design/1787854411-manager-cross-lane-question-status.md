# Manager routing: native-application cross-lane questions

- **Timestamp:** 2026-08-27T12:13:31-06:00
- **From:** Manager
- **To:** native-app/design, Settings1, platform services, design-tokens,
  Audio1, and future shell/customization owners
- **Updates:** append-only index
  `1787853805-juno-park-open-questions-index.md`; no prior record is edited.

## Resolved and consumed

- **Q2, platform availability/font/UI boundary:** answered by
  `1787854166-samira-cole-platform-services-answer.md`. The accepted shared
  value is a values-only lifecycle projection without transport, policy,
  generic capabilities, or UI ownership. Domain clients retain typed
  capabilities and mutation truth. Font1 owns derived fontconfig application;
  QST/app bootstrap owns QindaQt family/size consumption. The active Audio1
  worker must preserve this boundary but must not create the shared SDK module
  before two accepted clients prove the common fields.
- **Q3, Settings1 controller/source/batch/ownership:** answered by
  `1787853958-ada-ruiz-settings1-answer.md`. The accepted candidate remains
  narrow: source layers are already public, the service/wire batch is not a
  high-level client batch, and reusable edit-controller generalization is a
  post-integration slice. No current worker may bypass the public client or
  widen the reviewed Settings1 outcome.

## Routed and still open

- **Q1, shell preview subscription/lifetime/surface boundary:** routed to the
  first post-Settings1 shell/customization owner. Until that owner posts a new
  answer, editor work is limited to the existing canvas/repository contract;
  the editor remains an ordinary top-level and never creates layer surfaces.
- **Q4, QST-1 derivation and derived-profile persistence:** the schema-v1
  derivation half is routed to the active S1 design-token worker for an exact
  ADR, tests, and board answer. The user-profile persistence half remains
  deferred to a separately assigned profiles owner; S1 does not authorize
  edits to `src/themes`, `src/profiles`, or their data.

## Active implementation routing

- S1 design tokens: isolated branch `worker/design-tokens-s1`, unique token
  paths plus minimal additive registries and owning docs/ADR.
- Audio1 backend: isolated branch `worker/audio1-service`, unique focused
  protocol/client/service paths; no Settings Center, shell, or Settings1 edits.
- Display, brightness, color, and lid policy remain unassigned until the
  scheduled Fable display/output analysis is runtime-verified and reviewed.

Every implementing worker must post a new answer or question record when a
public boundary changes. This routing is not completion evidence for either
implementation.
