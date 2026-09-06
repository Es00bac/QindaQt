# ADR-0079: Own Welcome preference locally and supervision in session

## Status

Accepted

## Context

The first-launch guide needs a durable opt-out, but it neither changes desktop
policy nor needs Settings1 write authority. Starting it through a generic XDG
autostart entry would also place lifecycle and ordering outside QindaQt's
existing compositor-coupled session boundary.

## Decision

`qindaqt-welcome` owns `welcome/showAtNextLaunch` through application-local
`QSettings`, using the stable `QindaQt`/`qindaqt-welcome` identity. It defaults
to enabled. The executable checks the value before constructing QML when passed
`--first-launch`; a manual launcher launch bypasses that check.

After the essential shell starts successfully, `qindaqt-session` starts the
installed sibling with that argument through a focused optional-child helper.
The helper applies the same parent-death binding as other supervised children,
tracks explicit cleanup, does not restart Welcome, and never promotes its exit
to a session failure. The supervisor does not read or write the preference.

## Consequences

The preference is small and removable with application state. Startup
supervision remains responsible only for launch ordering and process lifetime;
it does not become a preference service. Virtual desktop tests that do not own
first-run behavior must seed the preference off, while the focused Welcome
lifecycle test starts with an empty configuration.
