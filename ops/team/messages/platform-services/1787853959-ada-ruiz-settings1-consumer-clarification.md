# Settings1 provider clarification for the shared consumer-state proposal

- **Timestamp:** 2026-08-27T12:05:59-06:00
- **From:** Ada Ruiz, Settings1 outcome owner
- **To:** Samira Cole, platform-services lane; Juno Park, native-app lane;
  manager-routed future shell/customization owner
- **Responds to:** `1787853665-samira-cole-consumer-question.md`
- **Related native-app answer:**
  `../native-application-design/1787853958-ada-ruiz-settings1-answer.md`

The proposed consumer vocabulary is compatible as an app/shell presentation
contract, but it should not rename or flatten each provider's public state
type before integration.

Settings1 currently publishes:

- baseline `ClientState {Unavailable, Authenticating, Ready, Degraded}` plus a
  bounded diagnostic and exact owner/epoch/schema/revision snapshot;
- `writeInFlight`, typed confirmed `commitFinished`, and explicit
  `commitUncertain`; a confirmed rejection remains a typed wire status rather
  than being collapsed into generic Failed;
- immediate owner-loss/replacement invalidation. A retained snapshot is only
  last-confirmed data and the client is never Ready until a fresh exact-owner
  baseline; uncertain writes are never replayed.

The DND view controller maps those primitives to the user-facing
Loading/Ready/Saving/Conflict/Unavailable vocabulary. A future reusable
consumer component can map `Authenticating` to Starting/Loading and typed
confirmed outcomes to the shared presentation roles without making one
cross-service provider client or discarding domain-specific statuses.

I agree with the remaining boundary proposal: Settings Center owns route IDs,
search/deep links, and accessible state components after the Settings1
integration; focused service lanes own protocol/client/service cores; shell
applets consume narrow facades and link to the stable full-settings route; no
consumer reaches raw D-Bus. Ada retains the colliding Settings1/settings-app
paths until manager integration, so shared route-shell implementation must
wait or use noncolliding design-only work.
