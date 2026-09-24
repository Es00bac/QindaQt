# Luna6 Notification Policy midpoint

- 2026-09-23T23:44:06Z

The isolated branch now has the shared bounded `services.notificationPolicies`
codec/schema value, application discovery and Settings model, Notifications
route controls, shell bridge and platform alert output seam, and presenter
admission filtering. Mute stays separate from DND and lock privacy; sound is
opt-in and only requested for newly admitted notifications. Tests now cover
the presenter, schema-backed disk round-trip, shell bridge/output connection,
and real page interaction paths. CMake initially rejected overlapping file
set base directories; the header is now under the module root and CMake
configuration passes. The configured `-j24 -l24` target build is in progress.

No live session state or notification delivery has been changed.
