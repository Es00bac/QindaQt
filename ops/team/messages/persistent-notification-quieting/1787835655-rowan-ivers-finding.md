# High: resident Settings1 omits the authoritative profile-default layer

Candidate `00b3d49ac3d7ba94edcf10272fa5e61185d63b56` does not implement
the documented four-layer generic settings authority. In
`src/services/settings_service/src/resident_settings_service.cpp:87-104`,
startup constructs `LayeredSettings` from schema system defaults and applies
only the user-overrides file. `src/services/settings_service/src/main.cpp:34-47`
passes the two schemas and user path, but no selected/installed profile-default
document. The production service therefore never loads
`data/settings/profile-defaults/qindaqt.json`, even though this candidate moves
that persisted layer to v2.

This is observable through the public generic protocol: with no user override,
`GetSnapshot(["appearance.animationDurationMs"])` reports the schema system
default `180` and source `system-defaults`, while the shipped QindaQt profile
sets `160` in `data/settings/profile-defaults/qindaqt.json:5`. Removing a user
override likewise reveals the wrong next value/source. That contradicts the
normative precedence and sole-authority claims in
`docs/wiki/architecture/settings-service.md:3-25` and the candidate's generic
Settings1 contract; it is not merely a future UI omission.

The passing persistence test only loads the profile file into a standalone
model (`tests/settings/tst_settings_persistence.cpp:35-46`). Service lifecycle
and real-transport tests never query a key whose profile default differs from
the schema default, so they cannot detect the production omission. Repair
needs an explicit validated profile selection/input at the service composition
boundary, application of that layer before user overrides, migration/rejection
semantics for persisted profile documents, and a private-bus snapshot/remove
test proving the profile value and `profile-defaults` source.
