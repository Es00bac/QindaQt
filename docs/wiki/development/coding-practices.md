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

The permitted dependency direction is documented in
[Module boundaries](../architecture/module-boundaries.md).

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
