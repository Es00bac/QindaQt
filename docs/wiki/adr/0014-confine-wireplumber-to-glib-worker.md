# ADR-0014: Confine WirePlumber to a GLib worker

- **Status:** Accepted
- **Date:** 2026-08-27

## Context

QindaQt's resident service and public client use Qt and QtDBus, while
libwireplumber 0.5 is a GObject library whose core, proxies, object manager,
plugins, async completions, and signal delivery belong to a GLib main context.
Moving those thread-affine objects between Qt and GLib event loops would make
ownership implicit and reconnect/teardown races difficult to prove. Running a
second WirePlumber instance as a policy manager would conflict with the
session's upstream authority.

## Decision

The Audio1 process keeps D-Bus ownership, public client state, protocol
validation, and publication on its constructing Qt thread. A dedicated
standard thread owns one private `GMainContext` and all libwireplumber/GObject
state. Only immutable, bounded Audio1 values cross queued boundaries. Requests
cross as typed value copies and are resolved again by `(epoch,
object.serial)` on the worker.

The worker connects as an ordinary observer/controller client to the running
PipeWire graph. It loads WirePlumber's public mixer and default-node APIs and
uses default metadata for stream targeting. It neither transports samples nor
loads policy scripts, monitors hardware, starts another policy daemon, parses
`wpctl`, or changes user/system PipeWire configuration.

Core disconnect cleanup is deferred outside the disconnect signal emission by
an explicitly retained idle source. The source carries its worker-run token;
cleanup destroys it and clears ownership synchronously, while dispatch rejects
any stale run. A discarded idle therefore cannot leave a latch that poisons a
later start.
Stop synchronously quits and joins the worker before backend destruction.
Component API loads begin only after PipeWire accepts the core, and both load
and core-sync completions own tracked cancellables. Cleanup cancels them and
stop keeps the GLib loop alive until every completion has released that state.
Queued worker tasks have GLib destroy notifications so context teardown cannot
leak a task that never ran. Immutable callbacks queued
to Qt carry a backend-run generation; stop invalidates it before the join, and
the coordinator rejects stopped or superseded generations. Restarting the same
adapter advances the service epoch before new publication, so a prior run's
handles and values cannot revive.
WirePlumber daemon or PipeWire authority replacement advances the service
epoch and makes pending mutations uncertain. Such mutations are never replayed.

## Consequences

- No `WpCore`, proxy, plugin, metadata, iterator, `GVariant`, or other GObject
  handle crosses to Qt.
- Backend tests can replace the platform adapter with a pure fake while
  production qualification exercises the same public boundary against a
  disposable WirePlumber/PipeWire runtime.
- The process pays for one extra thread and both Qt and GLib event-loop support.
  Snapshot rebuild work is linear in the observed bounded graph apart from
  upstream API costs; real-session CPU/PSS budgets remain unqualified.
- The public D-Bus schema remains independent of WirePlumber object layouts and
  bound IDs. An upstream API incompatibility can be repaired inside the adapter
  without changing consumers when semantics are unchanged.
- A future native application and shell facade consume the typed Audio1 client,
  not platform handles or the service implementation.

## Alternatives considered

- **Drive GObject handles from the Qt main loop.** Rejected because thread and
  context ownership would be implicit across D-Bus publication and teardown.
- **Expose WirePlumber proxies to Qt.** Rejected because queued delivery cannot
  preserve GObject affinity and would leak transient IDs into the public model.
- **Run a separate helper process for the GLib adapter.** Rejected for this
  version because Audio1 already provides the process crash boundary and value
  queueing gives a testable ownership seam without another protocol.
- **Invoke `wpctl` and parse output.** Rejected because text is not a stable or
  typed API and cannot provide trustworthy operation completion or lineage.
- **Become a policy manager.** Rejected because upstream WirePlumber is the
  runtime authority and competing policy would create oscillation and hidden
  configuration ownership.
