# ADR-0006: Make applet instance identity global within a profile

- **Status:** Accepted
- **Date:** 2026-08-26
- **Owners:** Profiles and Shell customization
- **Supersedes:** None
- **Superseded by:** None

## Context

Profile schema v1 originally described applet IDs as unique only within one
panel. That scope is too narrow for direct manipulation. Moving an applet
between panels must preserve its identity, while settings, undo history,
keyboard focus, drag anchors, and later persistence all need one stable key.
Making those systems interpret `(panel ID, applet ID)` as identity would turn a
move into an identity change and would spread compound-key policy through every
consumer.

A panel selected with `output: "*"` is expanded into one surface per output.
Those surfaces display the same declarative panel and applet instances; they do
not create new persistent applet identities.

## Decision

An applet `id` is a case-sensitive instance identifier and must be unique
across the complete profile. It is distinct from `plugin`, which identifies the
applet implementation. Panel IDs and both applet identifier fields are
non-empty and may not contain surrounding whitespace.

Moving an applet within or between panels retains its instance ID. Duplicating
an applet allocates a new profile-global instance ID. A wildcard panel retains
one persistent applet ID; a runtime occurrence may use `(applet ID, output ID)`
when it needs to distinguish the rendered copies.

The authoritative profile validator rejects the second occurrence of a panel
or applet ID before publishing any profile value. JSON loading, programmatic
editor candidates, and future migrations use that same validator. The error
identifies the repeated occurrence with an RFC 6901 path and carries the panel
and applet IDs when available.

This is a schema-v1 contract correction made before QindaQt has a released
profile compatibility baseline. Existing built-in profiles already satisfy it,
so no migration is required. After a compatibility baseline is released, any
incompatible identity change requires a new schema version and tested
migration rather than silently weakening this rule.

## Consequences

- Settings, history, accessibility, and drag/drop state can key an applet by
  one stable instance ID without importing panel placement policy.
- Cross-panel moves cannot accidentally collide with a hidden instance on the
  destination or another panel.
- Importers that previously reused familiar IDs such as `clock` on multiple
  panels must allocate distinct instance IDs before a profile is accepted.
- Profile validation must scan the complete panel graph, not validate each
  panel as an isolated persistence unit. The layout-only panel validator still
  ignores applets because geometry does not own applet identity.

## Revisit when

Revisit only if profiles gain an explicit hierarchical identity type whose
persistent and runtime-copy semantics replace string instance IDs throughout
profiles, customization, settings, and shell surfaces. Do not restore
panel-local IDs for the convenience of one importer.
