# Customization and visual design

Three kinds of data cooperate: a **profile** describes workflow and panel layout,
a **theme** supplies appearance inputs, and **settings** hold confirmed user
preferences. Keeping these separate lets a person use the same workflow with a
different palette or the same palette with a different panel arrangement.

## Profiles

The ten built-in profiles span QindaQt, minimal, GNOME-, Unity-, MATE-, XFCE-,
NeXTSTEP-, macOS-, and classic/modern Windows-inspired arrangements. These are
workflow inspirations, not implementations of those other desktops. Exact IDs,
panels, applets, and output selectors appear in the
[packaged assets catalog](catalog/assets.md).

Profiles are declarative JSON. The loader rejects malformed types, duplicates,
unsupported schema majors, excessive nesting, and identities that cannot round
trip. Applet instance IDs are unique across the profile: moving preserves an ID,
while duplicating creates another. Built-ins remain immutable; edits produce
user profiles. Accepted product concepts may be broader than schema v1, so only
fields in the [profile schema](../reference/profile-schema-v1.md) are persistable.

The layout planner uses desktop-logical coordinates. Qt handles conversion to
output buffers once; the planner must not scale panel dimensions a second time.
Multi-output expansion, stacking, collisions, and exclusive work areas are
validated as a whole before publication.

## Editing

Direct customization uses preview, commit/cancel, revision fencing, and undo/redo.
The editor domain owns gesture and keyboard intent; persistence adapters own
atomic profile storage; Settings1 owns confirmed profile selection. Pending,
failed, or conflicting saves cannot become confirmed UI state merely because
a preview rendered successfully. See [editor domain](../shell/customization-editor.md)
and [Customize Settings](../apps/customize-settings.md) for current canvas scope,
rollback, gesture behavior, and package boundaries.

## Themes, typography, and accessibility

Five shipped themes cover light, dusk, dark, high contrast, and the mist-and-sage
Qinda macOS palette. Theme schema v1 contains colors, typography inputs, icon
theme, corner radius, motion duration, and blur preference. A field being
accepted does not establish that every compositor effect is implemented.

QST-1 converts validated themes and accessibility inputs into immutable semantic
tokens. Consumers ask for the meaning of a color or spacing value, rather than
hard-coding theme IDs or deriving their own palette. The QML facade is read-only
and GUI-thread bound. Controls renders token-driven states, focus, and accessible
semantics. Font discovery is confined to its provider; confirmed preferences
reach supported first-party composition through documented startup boundaries.

See [design tokens](../architecture/design-tokens.md), [Controls](../shell/controls.md),
[fonts](../architecture/font-preferences.md), [icons](../shell/iconography.md),
and [theme schema](../reference/theme-schema-v1.md). The [privacy guide](privacy.md)
explains preference persistence; the [handbook index](index.md) links all topics.
