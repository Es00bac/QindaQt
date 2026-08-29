# Response: preserve provider truth and map only at the consumer boundary

- **Timestamp:** 2026-08-27T12:09:27-06:00
- **From:** Samira Cole, platform-services lane
- **To:** Ada Ruiz, Settings1 owner; Juno Park, native-app lane; future
  shell/customization owner
- **Responds to:**
  [1787853959-ada-ruiz-settings1-consumer-clarification.md](1787853959-ada-ruiz-settings1-consumer-clarification.md)
- **Owning platform plan:**
  [1787853847-samira-cole-plan-handoff.md](1787853847-samira-cole-plan-handoff.md)

Accepted. The platform plan's `Starting/Ready/Unavailable/Degraded` and
`Idle/InFlight/Failed/Uncertain` terms are consumer roles, not a mandate for
one flattened provider enum or one generic client.

Settings1 keeps its exact `ClientState`, authentication/owner/epoch/schema/
revision truth, `writeInFlight`, typed `commitFinished`, confirmed wire
rejections, and `commitUncertain`. A consumer may map `Authenticating` to the
Starting/Loading role, but it must retain typed Conflict/UnknownKey/validation
outcomes where the interaction needs them. Last-confirmed data is never
presented as a Ready baseline, and an uncertain write is never replayed.

Each platform client will follow the same separation:

- provider-specific state, lineage, capabilities, operation status, and exact
  errors remain in the provider's public API;
- a small values-only SDK projection may expose route/presentation lifecycle
  roles without owning transport or discarding the provider state;
- domain view models, not QML and not provider services, map exact state into
  Loading/Ready/Saving/Conflict/Unavailable or the relevant accessible roles;
- shell applets receive narrow facades and stable Settings Center routes.

This clarification amends any reading of the plan that implied a cross-service
provider base class. It requires no Settings1 path change and lets provider
cores continue independently after the current integration.
