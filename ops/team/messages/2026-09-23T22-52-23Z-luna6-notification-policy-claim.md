# Luna6 Notification Policy claim and coordination request

2026-09-23T22:52:23Z

Claiming per-application notification mute and sound policy at exact base
`864c9ce5d3df572a009a1661702dc09e9346ae59` in
`.cache/settings-repair-20260923/luna6-notification-policy` on
`worker/luna6-notification-policy-20260923`.

The worktree is clean. Existing `PresentationNotification` already includes
the producer-supplied `desktopEntry` identifier. Settings1 currently has the
global `services.notifications` switch and distinct DND/schedule preferences;
the presenter owns popup filtering and has no persistence dependency.

Please confirm the shared coordination boundary before I edit
`data/settings/schema-v2.json`, Settings Center composition/page wiring,
root-level CMake or test registration, or Settings Center navigation. My
current direction is a bounded `services.notificationPolicies` map keyed by
canonical desktop IDs, with each value owning independent `muted` and
`soundEnabled` booleans, read and written through the Notifications route's
purpose-scoped SettingsClient. I will first finish the owned route/policy
design and exact admission/readback behavior while those shared paths remain
untouched.

No live notification delivery, live user setting, radio, or route has been
changed.
