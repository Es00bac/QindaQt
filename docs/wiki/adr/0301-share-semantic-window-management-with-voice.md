# ADR-0301: Share semantic window management with voice

- Status: Accepted
- Date: 2026-09-29
- Supersedes: None

## Context

The owner requires voice alongside mouse, keyboard, touch, pen and native
applications. Existing Gabbee dictation and command modes already own capture,
recognition and the command hotkey. Commands must cover composable subjects and
placement, including named containers, appearance, tabs/splits, application
launches and inset maximize. The examples are not an exhaustive vocabulary.
Native application placement already has narrow same-connection surface authority
under [ADR-0298](0298-native-application-window-placement.md).

## Decision

Create a small Qt Core semantic command/policy library with explicit borrowed
authority, scene and executor interfaces. Typed requests contain an operation,
a bounded target and operation-specific arguments. Speech parsing stays in
Gabbee; platform mutation stays in the compositor adapter and existing atomic
Hybrid/scene boundaries. No raw transcript, script or simulated key sequence
acts as a compositor management request.

Bind command-mode capture to an expiring, single-use foreground context. The
production native endpoint authenticates the exact current Voice1 unique owner
and confirmed current-owner Settings1 opt-in. Revocation, lock and stale subject
state reject without guessing another target. Dictation never enters this path.
Ambiguity is an explicit result, and a refused recognized management command is
not typed into the application as text. Native app surface authority remains
unchanged and cannot be widened through this first-party voice boundary.

Use normalized usable-output rectangles for placement; 90% maximize leaves a
5% border on each side. Preserve restore state through the placement owner.
Installed desktop application identities, bounded correlated window arrivals
and truthful dispatch/completion results belong to a separate launch adapter.
No arbitrary command line or caller-supplied PID is a launch/management authority.
Only actually implemented native operations appear in capability replies.

## Consequences

The value/policy layer is independently testable without speech models, live
windows or hardware. Input adapters share semantic operations and existing
model invariants while keeping their own gesture/recognition responsibilities.
The compositor endpoint and native executor require private-bus, actual nested
scene and lock-generation gates before delivery; initial policy tests alone are
not deployed voice behavior. Shared interactive placement can extend this
boundary with explicit preview/confirm/cancel without revising V1 surface authority.

See [Native semantic commands](../architecture/window-management-commands.md),
[Voice input](../architecture/voice-input.md) and
[Application SDK](../architecture/application-window-management.md).

## Source decomposition review

Adding fractional restore state brings `hybridcontainerplacement.cpp` to 558 nonblank lines. It remains the cohesive geometry/drag/restore owner; maximize policy and shade policy already live in separate translation units. Ordinary window fraction state lives in `KWinSemanticWindowPlacement`, semantic dispatch in its own session translation unit, and authority/transport/runtime in separate collaborators. Keep this controller below 600 lines and split a new responsibility before extending it further. Fraction changes and cancelled drags must preserve the original restore frame; failed scene reflow rolls back fraction/restore state together.
