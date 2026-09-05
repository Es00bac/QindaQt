# ADR-0072: Publish atomic authenticated task facts

- Status: Accepted
- Date: 2026-09-04
- Deciders: QindaQt architecture group
- Scope: compositor and production Task List fact boundary

## Context

The Task List's T0 model requires window identity, presentation state,
output/workspace scope, and container role/lineage from one generation.
`Compositor1.Windows`, `Outputs`, `ShellVisibilitySnapshot`, and `Containers`
publish independently fenced inventories. Joining them in the shell could pair
a window with stale scope or container authority, so T1 correctly remained
Degraded and the real panel listed no windows.

Window actions already authenticate the production shell by joining the D-Bus
caller's PID to the sole committed dock-surface owner. Task facts include
titles, focus, attention, and topology, so broadcasting their content or
change timing to every session-bus peer would expand the observation boundary.

## Decision

`org.qindaqt.CompositorShell1` additively exposes `TaskListSnapshot() -> ay`
and the no-argument `TaskListSnapshotChanged` invalidation. The existing
panel-owner credential join runs before the fact source is consulted.
Unauthorized callers receive one fixed compact response with no epoch,
revision, count, identifier, or title. After an authenticated read, change
signals are targeted only to that exact unique owner and panel-owner loss
revokes the binding.

Schema 1 carries one immutable `(epoch, revision)` generation containing every
managed normal window, its stable UUID, application identity and label, title,
task role, state flags, output and workspace references, and container ID. The
same value contains the referenced output/workspace ID inventories, container
revision/authority lineage, and the `(epoch, actionRevision)` fence used by
window actions. Counts, payload size, text, references, uniqueness, membership,
and state combinations are bounded and validated atomically. A hostile sample
retains the prior generation; an unsampleable runtime publishes typed
unavailability.

The shell T1 producer binds the method and signal to the compositor's exact
unique owner, accepts only a monotonic byte-stable lineage, and publishes T0
only after the complete schema validates. Owner loss/replacement clears old
truth; transport or decoding failure degrades it. Invalidation is coalesced
and drives a reread—there is no polling loop or cross-inventory join.

## Consequences

- The production Task List can become Ready from real compositor facts while
  T0 grouping, filtering, and intent semantics remain unchanged.
- `Compositor1` stays the unauthenticated diagnostic/development surface;
  container mutations remain on its existing compatibility bridge.
- Compositor and shell must apply the same schema limits and generation rules.
  Forward revision gaps are valid complete snapshots; epoch changes,
  regressions, and changed bytes at an equal revision fail closed.
- The development-only `ShellDevelopment1.Snapshot` schema remains version 1
  and gains required `taskList` phase, generation, and window-count evidence so
  contained boot cannot pass while the hosted applet is degraded or empty.

## Alternatives rejected

Joining existing public inventories cannot make their sampling atomic.
Extending `Compositor1.Windows` would expose task titles and topology to every
session-bus peer and still leave authorization separate from actions. Polling
would add latency and load without repairing coherence. Fabricating a single
output/workspace or dropping malformed windows would turn missing authority
into false desktop truth.
