# ADR-0168: a generated container name never displaces a real title

- **Status:** Accepted
- **Date:** 2026-09-16
- **Owners:** Platform (compositor container appearance, shared chrome)
- **Supersedes:** ADR-0163's badge label clause (the label format only; every
  other decision in ADR-0163 stands), and with it ADR-0139's label clause
- **Superseded by:** None

## Context

[ADR-0139](0139-identity-borders-focus-and-rolled-up-badge.md) gave
the rolled-up badge a reserved label rect of **48-140 logical pixels** and the
format `<container name> · <foremost tab title>`.
[ADR-0163](0163-generated-container-names-for-the-rolled-up-badge.md) then made
the session always supply a container name while shaded: the rename override if
one exists, otherwise a stable generated `Container N`, so the badge is never
anonymous.

Those two decisions interact badly. The badge prefixed the container name
unconditionally, so a container nobody had renamed rendered as
`Container 7 · Quarterly Planning Notes` inside 140 px. Elided right, that is
about `Container 7 · Qua…`: the generated placeholder — which carries no
information the user chose — consumed the label and elided away the page title,
which is the text the user recognises.

The user-visible result, as reported: "Name shows up in the container window,
disappears when rolled up, unrolling brings it back, needs to stay when rolled
up." The unrolled shared row paints the title with the full row width
available, so the title reads normally there; rolling up replaced it with a
placeholder plus a truncation.

## Decision

A generated name is a fallback, never a prefix. The chrome request and plan gain
`containerTitleIsGenerated`, set by the session exactly where it substitutes
`displayName()` for an absent rename, and the badge label resolves as:

| Container name | Page title | Badge label |
| --- | --- | --- |
| user rename | present | `<name> · <page title>` |
| user rename | absent | `<name>` |
| generated | present | `<page title>` |
| generated | absent | `<generated name>` |

ADR-0163's guarantee is preserved: the badge still never paints an empty label,
because the generated name is what fills in when there is no page title. What
changes is that the placeholder yields to real text instead of crowding it out.

The unshaded shared row is untouched: it keeps the override-only contract, so an
unrenamed container paints no row title and tabs remain the page identity.

## Consequences

- A rolled-up container shows the title the user can actually read: their own
  container name when they set one, otherwise the page title, and the
  `Container N` placeholder only when there is nothing else to show.
- The label rect stays 48-140 px. Widening it was rejected: the badge is meant
  to be compact, and the real defect was spending the space on a placeholder,
  not the amount of space.
- `containerTitleIsGenerated` is a plan input, so the Appearance preview and the
  renderer resolve the same label as the live badge.

## Revisit when

- A rename surface exists for pages as well as containers; the table above then
  needs a row for a user-named page under a generated container name.
- The badge gains a second text row, at which point name and page title can
  both be shown in full and this precedence rule becomes unnecessary.
