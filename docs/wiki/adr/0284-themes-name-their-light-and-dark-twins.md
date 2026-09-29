# ADR-0284: Themes name their light and dark twins

- **Status:** Accepted
- **Date:** 2026-09-28
- **Owners:** App appearance (resolver), Themes
- **Supersedes:** None (refines the scheme rule in
  [Design tokens](../architecture/design-tokens.md))
- **Superseded by:** None

## Context

`resolveAppearanceTheme` turns the selected theme plus
`appearance.colorScheme` into the effective theme. A forced `light` or `dark`
scheme keeps the selection only when it is of that kind and otherwise falls
back to the built-in `qinda-light` or `qinda-dark`; `system` keeps the
selection exactly. When
[ADR-0281](0281-split-deck-container-title-and-container-names.md) added dark
twins of the three Corner Bar treatments, choosing Qinda Marigold under a dark
scheme still produced the generic Smoked Plum theme, and following a dark
system preference kept the light Marigold. The owner asked for the matching
twin in both directions.

## Decision

1. A schema-v2 theme may author `variants`, an object with optional `light`
   and `dark` theme ids (the document-id grammar). It is data about the
   selection, not a second theme: nothing else reads it.
2. The resolver, in order:
   - a forced scheme keeps a selection of the right kind; otherwise it uses
     the selection's twin for that scheme when the twin is installed and
     really of that kind; otherwise it falls back exactly as before (the
     built-in, then any compatible theme);
   - following the system keeps the selection, except that a *known*
     platform scheme the selection does not fit uses its installed twin of
     that kind. An unknown platform scheme, a missing twin, or a twin of the
     wrong kind keeps the selection exactly as before.
3. The six Corner Bar treatments pair themselves (`qinda-marigold` ↔
   `qinda-marigold-dark`, `qinda-corner-teal` ↔ `qinda-corner-teal-dark`,
   `qinda-corner-violet` ↔ `qinda-corner-violet-dark`); both members of a pair
   name both ids. Every other theme is unpaired and resolves as it did.

## Consequences

- The Settings preview, the shell, first-party applications, the Qt platform
  theme and the compositor all resolve through the same function, so they
  agree on the twin. The stored `appearance.theme` never changes: the user's
  choice stays the light (or dark) theme they picked.
- The portal's `Settings` projection reads the *selected* theme for its
  accent, not the resolved one, so a portal accent can come from the paired
  theme's other half. It is unchanged by this decision. **Update (2026-09-28,
  same-round follow-up):** the projection now calls the same
  `resolveAppearanceTheme` and reads the resolved theme's accent and
  `high-contrast` variant, exactly like the Settings preview, shell, and
  first-party applications; see
  [Portal service](../architecture/portal-service.md#appearance-projection).
  Installation is still checked against the exact selected id first, so an
  unknown selection fails the projection rather than resolving to a fallback
  theme.
- A v1 document may not author `variants` (the v2 key rule of ADR-0206).

## Icon families of the twins

Built-in color themes now author distinct icon families from the QindaThemes
`qinda-icons` package (the table is in
[QindaQt icon theme](../shell/icon-theme.md)); twins may use different
families. Because a theme's family may not be installed,
`Themes::resolveIconTheme` now accepts the authored id only when it is an
installed, valid XDG theme under the canonical roots, and otherwise resolves
to QindaQt. The ADR-0280 order (user choice, authored id, QindaQt) stands.

## Verification

`qindaqt.app-appearance-resolver` (forced and system schemes both ways, an
unknown platform scheme, a missing twin, a twin of the wrong kind, unpaired
themes unchanged, and the shipped Corner Bar pairs loaded from `data/themes`),
`qindaqt.theme-formats` (only Corner Bar themes pair; strict `variants`
values; v1 rejection; round trip), the built-in contrast rows, which walk
every theme under every scheme, `qindaqt.icon-theme-catalog` (installed
authored family, missing authored family to QindaQt, user precedence), and
`qindaqt.portal-appearance-policy` (a forced dark scheme on a light Corner
Bar Teal selection resolves to the dark twin's accent; an explicit light
scheme keeps the base theme's own accent; an unknown selected id still fails
the projection outright).

## Revisit when

Themes other than Corner Bar ship light/dark pairs. (The portal projection
moving to the resolved theme, above, is done.)
