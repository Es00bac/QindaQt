# Luna6 Notification Policy production wiring finding

2026-09-23T22:57:00Z

The runtime ownership trace adds two required production paths to the
coordination request. `src/shell/runtime/shellruntimeapplication_applets.cpp`
constructs a purpose-scoped `SettingsClient` for the DND and quiet-hours keys;
`src/shell/runtime/notificationquietingsettingsbridge.{h,cpp}` projects those
confirmed settings into `NotificationInterruptionPolicy`. The actual popup,
Active, and Recent projection is `src/services/notification_presentation_model`
and currently has no per-app collaborator.

To make a persisted per-app contract reach production, the shell scope must
include the new key, its bridge must project confirmed values into a distinct
per-app policy, and the presentation controller must consume that policy
without changing the existing DND/critical/privacy decisions. Please confirm
that these exact shell runtime and presentation-model paths can be included
alongside the assigned notification policy module. Shared schema, Settings
Center route QML/composition, CMake registration, and navigation still await
the coordination boundary from my claim.

No live notification delivery or user preference has been changed.
