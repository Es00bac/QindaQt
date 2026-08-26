# Module boundaries

Module boundaries prevent the desktop from turning into one tightly coupled
shell process. Each module owns its implementation, public surface, focused
tests, and the wiki page describing its contract.

## Source ownership

| Area | Responsibility | Allowed inward dependencies |
| --- | --- | --- |
| `src/core` | Pure window-container domain model, mutations, invariants, and persistence-neutral values | Qt Core and the C++ standard library |
| `src/profiles` | Layout-profile schema, validation, migration, and built-in profile data | `core` only when shared value types are unavoidable |
| `src/themes` | Theme schema, validation, token resolution, and built-in theme data | Foundation utilities; never shell objects |
| `src/shell` | Qt Quick presentation and controllers consuming public domain/profile/theme APIs | `core`, `profiles`, `themes`, and public service clients |
| `src/compositor` | Small QindaQt integration layer around the KWin downstream and compositor protocol adapters | `core` and explicit KWin extension points |
| `src/services` | Settings, session, metrics, notifications, portals, and platform adapters | Shared interfaces and narrowly selected platform libraries |
| `src/sdk` | Versioned client libraries, schemas, manifests, and generated IPC bindings | Foundation libraries only |
| `src/apps` | First-party applications behaving as normal desktop clients | Public SDK and application-focused libraries |
| `tools` and `tests` | Isolated development harnesses, fixtures, integration scenarios, and verification | Public APIs; test-only hooks in test builds |

Not every planned directory exists yet. Add one only when its responsibility is
implemented; do not use placeholder modules to bypass a boundary.

## Dependency direction

- `core`, profile, and theme models never import shell, compositor, service, or
  application presentation code.
- The compositor publishes state and accepts validated atomic commands. The
  shell does not link to KWin private objects.
- The shell depends on service clients, not service implementations. Platform
  adapters never call QML objects.
- Applets and applications use the SDK and public IPC. They do not include shell
  private headers or assume a specific panel implementation.
- Tests use public APIs first. Test-only compositor/output controls must be
  excluded from production builds and clearly named as test interfaces.

Cross-process contracts carry explicit version, error, timeout, and restart
semantics. Persisted formats carry a schema version and migration tests. A new
dependency crossing these directions requires an ADR.

## Decomposition rules

Keep data model, mutation policy, serialization, IPC adaptation, and visual
presentation separate. A controller may orchestrate collaborators but may not
also become their storage, renderer, and platform adapter. Split components
when they gain a second reason to change; line-count limits in root
`AGENTS.md` are a final warning, not the definition of modularity.

Future-agent comment conventions and interface documentation requirements are
in [Coding practices](../development/coding-practices.md). Changes to these
boundaries follow the [documentation policy](../contributing/documentation-policy.md).
