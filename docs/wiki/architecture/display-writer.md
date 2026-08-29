# Display compositor writer

The `display_writer` module is the bounded D4 bridge from immutable Display1
apply requests to KDE's public output-management protocol. It preserves the
transaction authority fixed by [ADR-0016](../adr/0016-display1-transaction-authority.md):
Display1 owns QindaQt's preview/rollback transaction while KWin remains the
sole live-state and restore authority. [ADR-0050](../adr/0050-direct-kde-output-management-writer.md)
fixes this adapter's exact production path and deliberately narrow identity
support.

D4 is a writer boundary, not a complete production transaction composition.
The installed Display1 executable still uses an unavailable transaction port
even though D5 now provides a separate durable journal implementation, because
authenticated lock state, logind inhibition, restart composition, and nested
convergence proof remain separate outcomes.
The D4 code never touches the host display during build or deterministic tests.

## Components and dependency direction

| Component | Owns | Does not own |
| --- | --- | --- |
| `mapApplyRequest` | Pure translation from one Display1 apply value to a narrow compositor configuration | Protocol objects, current-state discovery, persistence, retries |
| `validateConfiguration` | Total structural validation at the injected compositor boundary | Availability or output discovery |
| `WriterTransactionPort` | One in-flight request, machine-lineage/token/request/owner fencing, timeout, and exactly-once deferred completion | Journal implementation, Display1 state transitions, compositor inventory |
| `OutputManagementPort` | Injected asynchronous owner/submit/completion seam | D-Bus, Settings, files, UI |
| Production adapter | Direct ownership of a private Wayland connection and public KDE output-management objects | KWin private ABI/configuration, libkscreen production authority, physical outputs |
| `JournalStore` | Injected synchronous typed store/clear seam preserving unchanged, durable, and post-commit durability-uncertain truth for Display1 | Filesystem policy; D5 implements it in `display_journal` |

The installed public headers expose only owning Qt values, abstract ports, and
factories. Generated protocol wrappers, Wayland objects, socket notifiers, and
the pinned XML remain private implementation. All objects and callbacks are
confined to the constructing Qt thread. The observer is borrowed; owned ports
and journal stores have constructor-visible lifetimes.

## Fail-closed mapping

The integrated D2 inventory can currently publish only connector-fallback
stable IDs and the observed current mode. D4 therefore accepts exactly:

- `conn:CONNECTOR` stable identities, mapped to an exact protocol device name;
- `current:WIDTHxHEIGHT@MILLIHERTZ` modes, mapped to one exact advertised
  device mode with equal pixel size and refresh; and
- finite scale `1.0..3.0` plus the closed Display1 transform enum.

An EDID-derived stable ID, opaque mode ID, duplicate connector, unknown output,
ambiguous protocol device name/UUID, missing mode, malformed scale/transform,
or out-of-bounds value fails before a protocol apply. D4 does not guess from a
label, runtime UUID, mode preference, or nearest refresh. Arbitrary advertised
mode selection and EDID-backed identity require a later inventory contract.

A complete topology must name the protocol's exact current device set. It must
have at least one enabled output, exactly one primary, contiguous unique
priority, canonical disabled fields, and an acyclic replication graph whose
sources exist and are enabled. The adapter sends enable, mode, position, scale,
transform, priority, replication source, and primary through protocol requests
available by management version 13.

A surviving-properties rollback is intentionally narrower. It may address a
subset of currently enabled devices and sends only mode, scale, and transform.
It never replays old enablement, position, priority, primary, or replication
state after hotplug. Disabled complete-topology values may retain their D1
mode/scale/transform, but those fields are not emitted while disabled.

## Asynchronous and owner fencing

Only one compositor configuration may be in flight. Before submission,
`WriterTransactionPort` records the exact tuple `(machine lineage, Display1
token, writer request ID, output-management owner generation)`. Completion is
deferred onto the owning Qt event loop, even when a hostile fake calls back
synchronously. Duplicate, late, wrong-request, wrong-owner, and old-lineage
replies are ignored.

An owner/global replacement, device-set change, transport loss, explicit stop,
machine-lineage change, or local timeout makes the pending result
`TransportUncertain`; it never authorizes a forward replay. Protocol rejection
and structurally malformed/unsupported values map to a deterministic rejected
result. Concrete protocol objects belonging to an invalidated global set are
released so a later owner is not permanently held behind a stale busy slot.

## Protocol source and compatibility

The two client XML inputs are copied exactly from Plasma Wayland Protocols
v1.20 and pinned in the boundary test:

| Input | SHA-256 |
| --- | --- |
| `kde-output-device-v2.xml` | `52f8dc89df7ea6b6fe3930ff5d215aadb0841b6e1bc4e3cc9335d8745649da84` |
| `kde-output-management-v2.xml` | `07582b4596e18b557d5ee6b22f35d2c4304fbd5bf5bdc65eb29c69a18ebac5dc` |

The client binds management through version 19 and device through version 20,
but refuses mutation when management is older than version 13 because primary,
priority, and replication must have one coherent contract. Updating XML,
version policy, or identity mapping requires compatibility review and an ADR
update; generated-source warning exceptions never weaken warnings on
hand-written code.

## Evidence and stopping point

Focused Debug and Release evidence is selected with:

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^qindaqt\.display-writer'
```

The mapper row covers accepted complete/surviving values and fail-closed
identity, mode, topology, scale, and transform mutations. The port row covers
exactly-once completion, hostile synchronous callback, owner replacement,
lineage change, late reply, timeout, concurrent request, stop, and journal seam
behavior. Boundary and poison rows pin the XML and prove platform/private
dependencies cannot escape the installed header surface.

These are deterministic and compile-time D4 evidence. They do **not** prove a
real KWin apply, callback-before-observation ordering, post-apply convergence,
mirror visibility, hotplug recovery, service restart recovery, physical
monitor behavior, or the packaged Display1 process as an operational writer.
Those claims require the contained nested matrix before hardware qualification.

The D5 store is deliberately downstream of this module: it derives from the
installed `JournalStore` interface and is passed to `WriterTransactionPort` by
the future resident composition. D4 therefore remains independently testable
with a fake, while D5 owns Linux file operations and canonical startup loading.
`WriterTransactionPort` forwards the typed mutation outcome unchanged; it never
turns post-commit uncertainty into Boolean success or failure. Neither module
discovers a user-state path.
