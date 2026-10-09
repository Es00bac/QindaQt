# ADR-0368: Merge managed applications without desktop-key authority

- **Status:** Accepted
- **Date:** 2026-10-08
- **Owners:** ApplicationCatalog and Shell launcher
- **Supersedes:** None; implements only the catalog values subset of proposed ADR-0352
- **Superseded by:** None

## Context

Android and Windows applications need ordinary category/search/pin identities
before a QindaTK manager and qualified runtime dispatcher can compose them.
Desktop-entry keys, copied package names and aliases cannot establish a managed
recipe, launch target or authenticated running window. The existing apps-side
catalog already depends on launcher L0, so adding a second parser or reversing
that dependency would violate [module boundaries](../architecture/module-boundaries.md).

## Decision

The Qt Core-only launcher L0 exports owning `ManagedApplicationBinding` and
`ManagedApplicationRegistration` values. A binding holds Android/Windows
origin, a canonical lowercase non-null registration UUID, positive `quint64`
registration revision and exact lowercase SHA-256 recipe digest. The origin
namespace and UUID derive the stable catalog ID
`qindaqt.managed.android.<uuid>` or `qindaqt.managed.windows.<uuid>`; recipe,
revision and display-name updates keep the same pin/search identity.

An explicit catalog build overload accepts complete caller-supplied managed
registrations. It rejects the entire managed contribution on malformed values,
duplicates, excessive UTF-8 display fields, unsupported enums, source/entry
bounds or incomplete native inspection. Ordinary XDG claims always mask the
same managed ID, including Hidden/NoDisplay/deleted or malformed overrides.
A visible override remains native and inherits neither origin nor managed
launch binding. Desktop parsing has no managed-identity extension key.

The neutral apps-side scanner offers the same merge through an explicit
overload. Any indeterminate filesystem scan withholds managed rows while valid
native entries remain usable. Managed rows retain no desktop text/path.
Search, categories, pins/recents and accessible origin descriptions reuse the
existing projections. Origin is a display fact about the supplied registration,
not a permission or a classification of a running window.

`LaunchIntent` carries the optional exact binding. The neutral whole-row launch
planner returns `LaunchSupport::ManagedRuntime` with that binding and empty
program/argv. Managed routes are not spawnable. Malformed/mismatched bindings,
retained native documents and currently unsupported file/action handoffs refuse
before any dispatch. The runtime owner must recheck its current registration
revision and recipe digest and resolve its own packaged recipe; elapsed time or
a catalog value never authenticates that owner.

## Consequences

This is a dependency-ready public product seam, not an authenticated catalog
producer or a working Android launcher. No transport, package reader,
materialization receipt, runtime dispatcher, QindaTK manager composition,
green/blue theme token, live window association, container journey or Audio1
integration is delivered here. Those remain the explicit next gates in the
[foreign application architecture](../architecture/foreign-applications.md).
The proposed ADR-0352 service contract remains proposed.

All modified public structs/enums belong to static libraries. Consumers must
rebuild together; this slice defines no wire or persistence schema. Existing
native function signatures and default visibility behavior remain available.
The short standalone catalog gate compiles the touched libraries and executes
focused managed identity/override/failure tests plus their native regressions.

## Revisit when

An admitted runtime publishes verified recipe/materialization registrations,
a qualified file/action bridge is added, or the QindaTK Applications manager
and origin-token renderer are composed over this public seam.
