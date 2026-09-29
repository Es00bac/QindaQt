# ADR-0298: Native application window placement

- **Status:** Accepted
- **Date:** 2026-09-29
- **Owners:** Compositor and application SDK
- **Supersedes:** None
- **Superseded by:** None

## Context

Applications need to place their own newly created windows into container tabs
or tiles. A browser can interpret Ctrl+T as a new browser window placed in a
container page. PID, application ID, a title, or a compositor window UUID is
insufficient proof that a requester owns both windows.

## Decision

Expose the optional, versioned Wayland global `qindaqt_window_manager_v1`.
Both surface resources must belong to the requesting Wayland connection.
An application creates its own content, chooses its shortcuts, and sends a
source surface plus its new independent surface. The compositor owns admission,
container identity, geometry, and one atomic topology/scene transaction.

The [application window management contract](../architecture/application-window-management.md)
defines bounds, status, fallback, cancellation, and lifecycle. A public Qt client,
raw Wayland protocol, installed CMake package, and example expose that boundary.
The existing Hybrid model remains toolkit-neutral. Optional activation flags
select the inserted page inside the candidate transaction; their defaults
preserve existing callers.

No shell-wide interception of application shortcuts is introduced. This is an
opt-in integration: an unmodified browser continues its existing behavior.
No post-commit context recovery or second focus operation can convert a
successful mutation into an apparent refusal. The scene transaction propagates
source context, validates ownership, and applies focus before publication.

## Consequences

Applications can use compositor containers without linking private compositor
or Hybrid internals. Surface identity is authenticated by Wayland object
ownership rather than caller-supplied names. Separate Wayland connections cannot
place one another's windows even if their processes share an application ID.

V1 deliberately requires ordinary restored windows. Requests are refused while
any container uses temporary member-focus presentation: every scene transaction
replans containers, and restoring that presentation before a fallible request
would change unrelated state on failure. Supporting such presentation later
requires a scoped scene transaction or rollback-aware presentation seam.

The SDK adds Qt Wayland Client and the already permitted shared KWayland client
library, without a Plasma service dependency. Generated protocol descriptors
are confined C; public policy remains C++ and Qt Core. Package verification must
exercise an external consumer of the installed SDK, ordinary desktop fallback,
positive native placement, and bounded negative/lifecycle behavior.

## Revisit when

An application needs to adopt an existing grouped target, independent windows
from another connection, or placement during fullscreen/member-focus mode.
Those require a new authority and rollback contract rather than weakening V1.
