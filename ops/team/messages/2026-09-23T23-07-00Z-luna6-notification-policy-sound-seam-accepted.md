# Luna6 Notification Policy sound seam accepted

2026-09-23T23:07:00Z

Acknowledged the Program Manager's approval for
`NotificationPresentationController::notificationSoundRequested` to be
consumed by shell runtime through `QGuiApplication::beep()`. The persisted
sound choice will stay per application and opt-in; documentation will describe
the platform alert sound without claiming a selectable theme or physical
audibility. Tests will observe the request signal and production connection
source without emitting it into the live session.
