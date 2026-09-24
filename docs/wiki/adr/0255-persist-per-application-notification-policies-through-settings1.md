# ADR-0255: Persist per-application notification policies through Settings1

- **Status:** Accepted
- **Date:** 2026-09-23
- **Owners:** Settings, notification presentation, and shell
- **Supersedes:** None
- **Superseded by:** None

## Context

Notification quieting currently consists of global Do Not Disturb and a quiet
hours window. Users cannot mute one application or opt into alert sound for
selected applications. Application preference must remain separate from
critical urgency and authenticated lock privacy, and both the Settings route
and shell presenter need one durable, confirmed contract. The normal
notification protocol already includes a producer-supplied desktop-entry hint;
it is suitable for preference matching but is not authenticated identity.

## Decision

Persist per-application rules in the schema-defined Settings1 key
`services.notificationPolicies`, a bounded object keyed by canonical desktop
entry IDs without `.desktop`. Each record has exactly two Boolean fields:
`muted` and `soundEnabled`. At most 256 rules may be stored. A rule with both
values false and an absent key both mean the default policy; writers omit
default rules. One shared strict codec rejects malformed records as a whole.

Settings discovers application identities through the public
ApplicationCatalog boundary and displays confirmed Settings1 values. It does
not apply an edit until Settings1 reports Applied and a same-owner, same-epoch
snapshot at or beyond that revision matches the requested value. Refused,
conflicting, malformed, or uncertain outcomes never become presenter policy.

The shell applies only an exact confirmed snapshot through its own bridge.
Mute suppresses that application's popup, including critical urgency, and its
sound while leaving Active and Recent projections intact. The separate DND
policy continues to suppress low and normal urgency while admitting critical
urgency. Lock privacy continues to suppress all presentation and sound. Sound
is opt-in, defaults off, and requests the platform alert output only for newly
arriving notifications admitted by all policies; baseline and replacement
snapshots do not replay it. No notification service or private presentation
wire fields change.

## Consequences

The Notifications Settings route and shell share one stable persisted value;
the shell does not depend on Settings QML or private service headers. Old rules
remain visible if their desktop entry is no longer installed so they can be
removed. Applications may supply an identity hint that matches a preference,
but that hint grants no service or presentation authority. Sound currently
uses Qt's platform beep facility; this contract does not select a theme or
guarantee audible output. Focused tests cover strict encoding, Settings1
refusal and uncertainty, real-page rollback, presenter admission, and a disk
round-trip through schema-v2 persistence.

## Revisit when

Reconsider the rule bound or record fields if the application catalog
establishes a different canonical identity contract, or if the platform
provides a selectable per-application sound asset contract.
