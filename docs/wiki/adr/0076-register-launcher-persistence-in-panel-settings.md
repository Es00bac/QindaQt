# ADR-0076: Register launcher persistence in panel settings

- Status: Accepted
- Date: 2026-09-05
- Supersedes: the persistence key names in [ADR-0062](0062-bound-launcher-execution-behind-injected-seams.md)

## Context

Launcher persistence requested `shell.launcher.pinned` and
`shell.launcher.recent`, but the shipped Settings schema defines no `shell`
domain or either key. Settings1 correctly rejects the complete scoped snapshot
when any requested key is unknown. Consequently launcher pins and recent
applications were never persistent in the production session, and the shared
shell client also lost its other settings.

## Decision

Use `panels.launcherPinned` and `panels.launcherRecent` in the existing panel
domain. Schema v2 declares each as a string list with an empty default.
The launcher controller continues to validate desktop-entry identities,
uniqueness, and its existing limits of 16 pins and 8 recent entries before
presenting or committing values. Generic wire bounds remain unchanged.

The document encoder also converts schema-normalized string lists to canonical
JSON arrays before encoding. Previously `QStringList` passed schema validation
but failed the stricter JSON encoder, so adding keys alone could not save pins.
Empty and populated lists have round-trip regressions retaining schema types.

No domain, transport, or generic schema type is added. The retired spellings
could not be written through the shipped service, so there is no supported
persisted value to migrate. Existing valid v2 documents acquire the empty
system defaults without rewriting unrelated user overrides.

## Consequences

A schema-backed private-bus test must exercise the actual runtime scope,
production persistence controller and Settings1 transport, then verify disk
contents, new-owner/new-epoch service-restart recovery and fresh shell-client
reload. A service epoch change under the same unique bus owner remains a
protocol regression and is rejected. Synthetic snapshots alone cannot prove
this cross-module contract. The public Settings service remains the sole
persistence authority; launcher code still does not access its storage.

See [Launcher](../shell/launcher.md) and
[Settings1 protocol](../reference/settings1-v1.md).
