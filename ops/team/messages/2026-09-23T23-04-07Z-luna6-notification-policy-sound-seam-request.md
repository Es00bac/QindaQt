# Luna6 Notification Policy sound consumer coordination

2026-09-23T23:04:07Z

Thanks for the authorization. I will use the new single
`services.notificationPolicies` object, keyed by canonical desktop IDs, and
keep app mute as popup-only. DND remains the urgency-aware popup interruption
rule; explicit app mute suppresses that app's popup, including a critical
popup; lock privacy continues to deny every presenter projection and sound.

There is currently no notification sound consumer in the production presenter.
To make the persisted `soundEnabled` field real without adding a QtGui
dependency to the public notification model, I propose that
`NotificationPresentationController` emit a new `notificationSoundRequested`
signal only for a newly arriving, otherwise-admitted, sound-enabled
notification, and that the shell runtime connect it to
`QGuiApplication::beep()` (QtGui is already owned there). The private presenter
test would spy on the signal and prove that mute and DND suppress it, while a
permitted sound-enabled app emits it. This adds a new output signal contract
from the presentation model to shell runtime, but no new library dependency.

Please flag if this consumer shape is not acceptable. No sound is emitted by
this worker while coding or testing; the output is isolated by the presenter
test and the production signal is only part of the candidate source.
