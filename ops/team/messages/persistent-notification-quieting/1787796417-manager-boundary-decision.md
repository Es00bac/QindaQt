# Manager boundary decision — 2026-08-27T02:06:57Z

This resolves the contract and UI audit questions before implementation. These
decisions refine `docs/TASK_LIST.md`; they do not broaden the outcome.

## Schema and migration

Ship an active schema v2 and retain schema v1 as an accepted migration input.
Add a dedicated boolean key such as `services.doNotDisturb`, default `false`;
do not repurpose `services.notifications`. Implement an explicit one-way v1 to
v2 document migration for valid persisted profile/user documents. Unknown,
corrupt, newer, or invalid documents fail without mutating authoritative state.

## Service authority and lifetime

`qindaqt-settings-service` is independently session-bus activatable. It is not
a third tokenized `qindaqt-session` child and has no compositor, notification
presenter, or lock authority. Settings writes are ordinary same-user session
operations; the client authenticates and fences one exact unique service-owner
lineage to prevent stale replies/signals, not to attest an executable or create
a security boundary against the user.

Service-side persistent commits must be copy-on-write: validate and apply to a
candidate model, atomically save the candidate document, then publish/swap the
authoritative in-memory model and revision. A save failure must leave memory,
disk, revision, and change publication unchanged.

## Client and shell failure policy

Before the first authenticated Settings1 baseline, the shell remains quiet for
low/normal popup admission. This prevents a persisted `true` choice from being
briefly violated during shell startup. After a baseline, owner/service/bus loss
holds the last confirmed DND value while the client reauthenticates. An
uncertain write is never replayed automatically. Replacement-owner state is
applied only after a complete authenticated snapshot.

This conservative pre-baseline state affects interruption only; Active/Recent
retention follows existing DND policy, and the independent lock privacy gate
continues to deny every projection/action unless conclusively unlocked. The
settings client can never relax privacy.

## User interface boundary

Implement `qindaqt-settings` as a separate ordinary Qt Quick application with
a stable `notifications` route and `--page notifications`. It consumes only a
public asynchronous Settings1 client/view model and must not link shell or
notification presentation internals. Install its desktop entry, and add a
focusable **Notification settings…** action to the existing notification center
through a narrow fixed-route launcher. Do not add a settings applet, global
shortcut, or arbitrary process-launch capability.

The existing notification-center quick toggle may remain only when it submits
through the same Settings1 transaction controller. Make presentation DND
read-only to QML. Both surfaces expose honest Loading, Ready, Saving, Conflict,
and Unavailable/error states; conflict refreshes and requires explicit retry,
and timeout/transport uncertainty never claims persistence.

## Evidence interpretation

“Independent shell restart” means disposable process-harness proof while the
service remains, not a change to the current essential host/shell supervisor.
Transaction tests own no-op/conflict/rollback/malformed/persistence failure;
client/transport tests own exact-owner replacement/timeout/bus loss. Offscreen
QML proves structural keyboard/accessibility semantics only, not a live
assistive-technology or compositor run.
