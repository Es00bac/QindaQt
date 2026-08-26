# ADR-0008: Own a lean QtDBus notification service

- **Status:** Accepted
- **Date:** 2026-08-26

## Context

A complete QindaQt session must own the standard
`org.freedesktop.Notifications` name, retain bounded notification state, expose
actions to the shell, and survive malformed or hostile clients. Qt's KDE
Frameworks notification library is a client API rather than the session
server. Plasma's notification-manager implementation brings Plasma workspace,
KIO, activities, task-manager, widgets, and X11 dependencies into the resident
desktop process.

QindaQt targets a small resident memory footprint and already separates
toolkit-neutral policy from platform adapters. The freedesktop
[Desktop Notifications Specification 1.3](https://specifications.freedesktop.org/notification/latest/)
has a small stable D-Bus surface that QtDBus can implement directly.

## Decision

QindaQt will own a modular notification service rather than importing Plasma's
runtime implementation.

`QindaQt::Notifications` is a Qt Core domain/service library. It owns bounded
typed requests, immutable revisioned snapshots, replacement and ownership
policy, expiration, close reasons, action invocation, and injected clock and
presentation-backend seams. It owns no D-Bus connection, QTimer, QML object,
sound player, or persistence store.

`QindaQt::NotificationDBus` is a separate QtDBus adapter. It explicitly
acquires `org.freedesktop.Notifications`, registers the standard object path,
authenticates requests by the caller's unique bus name, decodes bounded
standard hints, and emits the standard close/action signals. Its injected
presentation backend is a host-side composition seam and test boundary; it is
not a cross-process shell transport.

Before user-visible presentation lands, the resident host will expose a
versioned private QindaQt adapter and the shell will consume its corresponding
client. That contract must publish revisioned snapshots and carry trusted
dismiss/action commands with explicit authentication, errors, timeouts,
backpressure, restart, and resynchronization semantics. The producer-facing
freedesktop object cannot substitute for this private contract, and the shell
must not link the notification service implementation merely to obtain model
updates.

The adapter reports specification version 1.3. Its default capabilities list
contains only `body`. A composed host may advertise `actions` only when an
accessible action UI is actually connected; markup, hyperlinks, inline body
images, sound, and persistence remain unadvertised until implemented.

Protocol inputs are admitted atomically. Text, action, hint, image, active
count, and aggregate retained-payload limits bound memory before publication.
Independent active-count and retained-payload limits keyed by the authenticated
unique sender prevent one source from filling all persistent capacity.
Notifications are not reclaimed merely because that sender disconnects, since
short-lived producers legitimately leave useful notifications behind; global
bounds remain the defense against coordinated or reconnecting identities.
Rejected requests preserve the prior snapshot and revision. Backends may read
the published immutable snapshot during a callback, but reentrant mutation is
rejected.

## Consequences

- The resident service depends on Qt Core and QtDBus, not Plasma shell.
- Shell presentation, history UI, do-not-disturb policy, timers, sounds,
  persistence, and lock-screen redaction can evolve behind explicit injected
  boundaries.
- The current host foundation has no shell-facing cross-process client. Popup
  work is gated on the versioned private adapter/client above; the in-process
  presentation backend alone does not qualify process decoupling.
- A second desktop already owning the bus name is a visible startup failure;
  QindaQt does not silently run two notification servers.
- Caller ownership prevents one live client from replacing or closing another
  client's notification ID. An unknown nonzero `replaces_id` still uses and
  returns that exact ID as required by specification 1.3.
- Server-generated IDs remain monotonic and skip active explicit identities.
  Closed caller-supplied IDs are invalidated by the protocol and are not kept
  in an attacker-fillable global lifetime history.
- Default per-source ceilings reserve persistent-notification and retained-byte
  headroom for unrelated bus peers without changing replacement or
  post-disconnect persistence semantics.
- Private-session-bus tests are required because a developer's current desktop
  normally owns the well-known service.
- Portal notification routing and restart/migration behavior remain later
  Platform-services qualification rather than hidden inside this foundation.

## Alternatives considered

- **Use Plasma's notification manager.** Rejected because its transitive
  runtime conflicts with the lightweight shell goal and couples policy to
  Plasma presentation.
- **Use the KDE Frameworks notification client as a server.** Rejected because
  it does not own the desktop-server protocol.
- **Forward every request to a popup process without retaining a model.**
  Rejected because replacement, expiration, history, actions, restart policy,
  and resource limits need one authoritative session model.
- **Implement notification presentation in the D-Bus object.** Rejected because
  it would merge untrusted protocol decoding, policy, timers, and QML into one
  process object that cannot be tested or replaced independently.
