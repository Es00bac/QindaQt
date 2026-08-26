# Settings model and service boundary

QindaQt settings use a validated, schema-versioned model before they cross a
process boundary or reach a control-panel page. The current C++ foundation owns
schema loading, layered resolution, optimistic transactions, change sets, and
atomic JSON persistence. The `org.qindaqt.Settings1` D-Bus service and visual
settings center remain later slices; neither may bypass this model.

## Resolution order

Settings resolve from highest to lowest precedence:

1. volatile session overrides;
2. persisted user overrides;
3. persisted profile defaults; and
4. system defaults declared by the schema.

Every schema key has one normalized system default, so an effective read of a
known key is never ambiguous. Removing a value from a higher layer reveals the
next value and records both source layers in the resulting change set. Session
overrides are never written to disk.

## Atomic updates

A caller begins a transaction against a mutable layer and captures the current
revision. It may stage any number of set and remove operations. Commit validates
the complete candidate layer, then either publishes every operation or publishes
none. A raw-layer mutation advances the revision exactly once; a semantic no-op
keeps the existing revision. A stale revision returns a conflict instead of
overwriting newer work. System defaults are read-only.

Successful changes identify touched keys and the subset whose effective value
or source actually changed. This is the future notification payload for
`org.qindaqt.Settings1`; presentation code should refresh only the owning pages
and live previews.

## Schema v1 domains

The shipped schema in `data/settings/schema-v1.json` covers appearance,
wallpaper and animation; font family, size, antialiasing, hinting and subpixel
order; displays and fractional scaling; pointer and keyboard input; panels;
window docking and focus; accessibility; and desktop-service preferences.
Definitions carry an exact type plus optional numeric ranges, enumerated values,
and non-empty constraints.

Persisted profile and user documents include `schemaVersion`, their exact
`layer`, and a `values` object. Loading rejects unknown keys, incorrect types,
invalid constraints, or an unknown/non-persistable declared layer. Saving uses
`QSaveFile`, so a failed write cannot leave a partially replaced settings
document.

## Process contract

The future settings service owns persistent files, migrations, revision order,
preview/commit/rollback coordination, and change notification. The settings
center is an ordinary client. Compositor and platform adapters consume scoped
changes through their public clients; they do not read the JSON files or link
to settings UI objects.

The component ownership is summarized in
[Architecture overview](overview.md), and dependency rules are in
[Module boundaries](module-boundaries.md).
