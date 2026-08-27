# ADR-0013: Own QST-1 semantic tokens and isolate optional Kirigami reuse

- **Status:** Accepted
- **Date:** 2026-08-27
- **Owners:** Design-system working group
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaQt's five schema-v1 themes provide a compact raw palette and metrics, but
first-party shell/application QML needs stable roles for state, focus, status,
spacing, type, motion, and elevation. Letting each control derive those roles
would create inconsistent accessibility and theme behavior. Adopting Kirigami's
theme and units stack as the authority would introduce a second palette model
coupled to KDE presentation conventions.

The owning contract is [QST-1 semantic design tokens](../architecture/design-tokens.md).

## Decision

QindaQt owns QST-1 as a pure derivation from public schema-v1 `ThemeSpec`
values and explicit caller-owned typography/accessibility inputs. Derived roles
are computed, never stored in theme files, so theme schema v1 and user-authored
theme compatibility remain intact.

The immutable C++ value and pure deriver have only themes and Qt Core/Gui as
inward dependencies. A separate GUI-thread `QindaQt.Tokens 1.0` adapter exposes
read-only role maps to QML; applications publish complete generations through
C++ composition. The adapter never reads Settings1 or grants mutation
authority to QML.

Qt Quick Controls 2 remains the future control foundation. Kirigami is not the
QindaQt application framework or token authority. A future feature may reuse a
Kirigami component only behind a thin QindaQt-owned controls adapter that keeps
Kirigami types and theme behavior out of application APIs. Such reuse requires
focused dependency, accessibility, resource, and replacement tests.

## Consequences

- Every first-party control has one token vocabulary across all built-ins.
- Schema-v1 theme authors receive derived roles without migration or duplicate
  per-application overrides.
- Font point size, text scale, reduced motion/transparency, and high-contrast
  preference remain caller inputs; the token layer has no persistence or
  service dependency.
- WCAG pair checks and deterministic transform/property tests are mandatory for
  compatible QST-1 corrections.
- QST/QML role compatibility is independent from theme-schema compatibility.
- Selective Kirigami reuse remains possible, but adapter ownership adds a small
  amount of deliberate integration code.

## Revisit when

Reconsider only when a concrete authored visual role cannot be derived from
schema v1, when third-party consumers require a versioned override mechanism,
or when measured maintenance/resource evidence shows a Kirigami adapter cannot
provide the required behavior without exposing a second theme authority.
