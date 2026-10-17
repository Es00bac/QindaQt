# Coding practices

QindaQt favors small, composable modules over a central desktop process full of
special cases. The root `AGENTS.md` is authoritative; this page explains how to
apply it during implementation.

## Keep responsibilities narrow

- Separate domain values and mutations from persistence, IPC, platform
  adaptation, and UI.
- Prefer explicit constructor dependencies and value-oriented messages. Avoid
  ambient singletons, hidden service locators, and cross-module mutable state.
- Put an interface in the module that owns the abstraction, not automatically
  beside its first consumer or implementation.
- Refactor when a type has multiple unrelated reasons to change. Do not wait for
  the file-size guardrail before decomposing it.
- Keep QML visual components focused on presentation. Business rules and
  persistent state belong in testable C++ domain or service components.
- Make cancellation, timeout, restart, ownership, and thread-affinity behavior
  explicit at asynchronous and process boundaries.
- Shell, Settings, Settings Center, and Welcome QML consumes semantic values
  through [QST-1](../architecture/design-tokens.md). Do not add theme IDs,
  palette hex literals, per-app token derivations, or a second framework theme
  authority. Bundled applications (Calendar, File Manager, Terminal, Text
  Editor) instead use stock Qt 6 controls themed through the Qt platform theme
  per [ADR-0116](../adr/0116-build-bundled-applications-on-stock-qt6.md). The
  image/PDF Viewer uses QindaTK under the explicit exception in
  [ADR-0218](../adr/0218-use-qindatk-and-poppler-for-the-viewer.md); Poppler
  remains private to its C++ document backend.

The permitted dependency direction is documented in
[Module boundaries](../architecture/module-boundaries.md).

## Link the module whose header you include

A target that includes a `<qindaqt/...>` header links the target that provides
it, and a library whose headers expose another module's types links that
module PUBLIC. Without the link the compiler can fall back to an installed,
possibly older copy of the same header in `/usr/include/qindaqt`, and the
object then disagrees with the library it links about class layout. That is
how the power service's lid and critical-battery runtime ran a corrupted
`SessionActionsClient` after the 2026-10-16 stability round.
[ADR-0388](../adr/0388-fail-the-build-when-a-qindaqt-header-resolves-outside-the-tree.md)
makes the build enforce the rule: `QINDAQT_HEADER_ISOLATION` (default ON)
places an `#error` stub for every QindaQt public header after all `-I`
directories, so a missing link fails at compile time. The
`qindaqt.header-isolation-guard` test proves the stub loses to a linked module
and stops an unlinked include.

## Document for a future agent

Assume the next maintainer is an AI agent with the repository but none of the
conversation that produced the code. Names and types should explain the normal
case. Comments should explain only constraints the code cannot communicate:

```cpp
// AGENT-GUARD: Publish only after validateTopology() succeeds; shell observers
// cache one generation and cannot recover from an intermediate unary split.
publish(nextTopology);
```

Use `AGENT-NOTE` for non-obvious rationale, `AGENT-GUARD` for a local invariant
and its failure mode, and `AGENT-CONTRACT` for a requirement shared across
modules/processes. Link durable rationale to the relevant wiki page or ADR.
Never use markers to excuse poor factoring or duplicate documentation verbatim.

## Verification by change type

Choose only the rows affected by the current slice. During coding, use the
smallest deterministic check that exercises its contract (30-second target,
60-second stop); build touched targets once after the code stabilizes. The
larger matrix is a release selection guide, not a list to rerun after every
edit. Preserve direct evidence for high-risk boundaries and state unavailable
coverage honestly.

| Change | Minimum evidence |
| --- | --- |
| Pure model mutation | Unit/property tests for valid and invalid transitions, plus round-trip persistence where applicable |
| Schema or profile | Validation, migration, malformed-input, and built-in-data tests |
| Shell component | Controller tests, keyboard path, and representative profile/resolution scenario |
| IPC or service | Contract, timeout, restart, version mismatch, and permission tests |
| Compositor/display | Nested deterministic scenario followed by relevant hardware coverage |
| Visual token/theme | Token validation and approved visual-baseline comparison |

Tests describe externally meaningful behavior rather than private call order.
Keep deterministic fixtures small and make performance measurements repeatable.

Behavior and architecture changes also follow the
[documentation maintenance policy](../contributing/documentation-policy.md).

## Commit boundaries

Completed roadmap milestones receive descriptive Git commits. Their messages
record the delivered contracts, validation evidence, and next dependency
boundary so a future agent can resume from repository history without the
conversation that produced it. Work for a later milestone begins only after
the completed boundary is committed.
