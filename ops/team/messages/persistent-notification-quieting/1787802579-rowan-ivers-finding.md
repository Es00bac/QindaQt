# High: unavailable activation retries have no delay or in-flight bound

Candidate `00b3d49ac3d7ba94edcf10272fa5e61185d63b56` can spin an unbounded
`StartServiceByName` loop when the Settings1 activation descriptor/executable is
missing or activation repeatedly fails. `SettingsClient` handles
`activationFailed` by calling `scheduleRetry()`
(`src/services/settings_client/src/settings_client.cpp:86-90`), but
`scheduleRetry()` immediately calls `requestActivation()` whenever the owner is
empty (`:398-405`); it neither starts the configured retry timer nor marks an
activation request in flight. Each asynchronous activation failure therefore
launches the next call immediately. Explicit UI refreshes can also stack more
activation calls because the transport has no activation-in-flight guard.

This contradicts the documented bounded activation/backoff behavior and can
consume CPU/session-bus capacity precisely in the required unavailable state.
The client tests cover snapshot retry timing and successful private-bus
activation, but no persistent activation-failure/recovery case. Repair needs a
single serialized activation attempt, configured backoff between failures, and
a test proving call count remains bounded during sustained failure and recovers
when an owner later appears.
