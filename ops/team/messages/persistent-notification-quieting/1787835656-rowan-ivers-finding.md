# High: synchronous client startup failure strands both settings surfaces in Loading

Candidate `00b3d49ac3d7ba94edcf10272fa5e61185d63b56` has no recoverable UI
state when the Settings1 transport cannot start (for example, a missing session
bus or failure to install the local disconnect subscription).
`DoNotDisturbController` begins in `Loading`
(`src/services/settings_client/include/qindaqt/services/settings_client/do_not_disturb_controller.h:57-64`)
and only synchronizes from later client signals. `SettingsClient::start()`
returns false at `src/services/settings_client/src/settings_client.cpp:120-124`
without publishing a state transition; its state was already `Unavailable`, so
there is no signal for the controller. Both production composition roots merely
log the returned error (`src/apps/settings_center/main.cpp:38-41` and
`src/shell/runtime/shellruntimeapplication.cpp:319-324`).

The ordinary settings page consequently remains forever on “Loading Do Not
Disturb setting…”, hides its Retry action because `unavailable` is false, and
offers no recovery. Even a direct controller `retry()` cannot recover because
it calls `SettingsClient::refresh()`, which returns immediately while
`m_started` is false (`settings_client.cpp:152-166`). The shell quick control is
stranded in the same dishonest Loading state, although its interruption policy
does remain safely quiet.

This blocks the required honest Unavailable/error state and explicit recovery
path. Existing UI tests use a fake projection and never make transport start
fail; client tests use a transport whose `start()` always succeeds. Repair
should publish/synchronize the initial failure, preserve a bounded diagnostic,
and make Retry perform a real serialized client/transport start attempt (with
the same activation backoff/in-flight guarantees), with controller and
offscreen-QML tests for failure then recovery.
