# ADR-0397: Observe controller consumers off the compositor thread

- Status: Accepted
- Date: 2026-10-17
- Owners: Controller runtime and compositor integration
- Amends: [ADR-0347](0347-controller-desktop-integration.md), observation scheduling only

## Context

Regular laptop video and screensaver hitches traced to the controller plugin's
one-second consumer poll. A twelve-second kernel trace measured twelve gaps of
28–29 display refreshes, with 438–454 ms compositor CPU runs and at most 0.024 ms
scheduler delay. A native stack caught `inspectGamePriority` inspecting process
FD links through `QFileInfo::symLinkTarget`. Directory filtering also follows
the links' targets, multiplying filesystem work and potentially reaching network
mounts. Running that work on the compositor thread stops all frame delivery.

## Decision

One controller-owned worker observes same-user process names and FD link text.
It runs with normal scheduling, independently of the compositor's realtime
policy. It never touches SDL, profiles, compositor objects or the input sink.
Runtime state and existing authority/persistence/Controllers1 boundaries stay
on the owning GUI thread. No new service or dependency is introduced.

Only one scan may run at a time. Periodic polls coalesce. Device changes and
open/close notifications release held desktop input immediately and invalidate
the current observation. Generation fencing discards a result invalidated in
flight and scans the latest path set. Initial discovery and invalidated input
wait for a current result; ordinary periodic polls retain the previous result
until their replacement arrives. A current result still has to pass the
existing Steam/game/session gates and neutral-control resume rule.

Read FD links directly with `readlinkat`, without following or canonicalizing
their targets. Skip vanished/inaccessible processes and descriptors as before.
Check interruption between entries. Destruction requests interruption and joins
the worker before plugin code can be unloaded. Queued results cannot outlive
their receiver.

## Consequences

Process inspection latency no longer delays rendering. A game/device transition
may briefly report `checking-game`; continuous desktop controls are not released
by each periodic poll. Steam still closes owned handles, existing consumers
still suppress their devices, and resuming controls must first be neutral.
The plugin retains its exact compositor ABI and existing profile format.

Focused verification covers a deliberately blocked scan with a live GUI event
loop, coalescing, stale-result rejection, normal worker scheduling, interrupted
teardown, FD links with missing targets, Steam and virtual controller resume.
Installed acceptance compares frame delivery on the same running compositor.

See [Controller desktop input](../architecture/controller-input.md).
