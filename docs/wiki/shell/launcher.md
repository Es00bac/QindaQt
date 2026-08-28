# Launcher

The launcher presents installed applications for browsing and activation. This
page records the bounded L0 model that exists today and the boundaries that
remain deliberately unimplemented.

The implementation lives in `src/shell/launcher` (namespace
`QindaQt::ShellLauncher`) and is a pure Qt Core model: it links no Qt Gui,
Qt Quick, Qt DBus, or KDE frameworks. The model never reads the filesystem,
environment, session bus, or processes; callers feed it desktop-entry text and
consume values. This boundary is accepted in
[ADR-0042](../adr/0042-launcher-model-without-execution.md).

## Values and validation

`DesktopEntryParser` turns one desktop-entry document into a validated value
or a typed error (`DesktopEntryErrorCode`). The accepted subset is the
freedesktop desktop-entry keys the launcher presents: `Type` (only
`Application` is launchable), `Name`, `GenericName`, `Comment`, `Icon`,
`NoDisplay`, `Hidden`, `Categories`, `Keywords`, and `Actions` with their
`[Desktop Action <id>]` groups. Unknown keys and locale-suffixed variants such
as `Name[de]` are ignored without decoding their payload, so an extension's
escape grammar cannot invalidate a document. Every key first requires a
non-empty ASCII key name and, when present, a complete syntactically valid
locale suffix. Recognized groups reject duplicate keys/groups, booleans accept
only `true` or `false`, whitespace around `=` is ignored, escaped semicolons
remain inside list items, and action IDs use the bounded ASCII key-name grammar.

Documents are hostile input. The parser and catalog enforce fixed ceilings
declared in `launcher_bounds.h` (QString document code units, field lengths,
keyword, and action counts). Violations produce typed errors — never
exceptions, partial entries, or unbounded growth.

`ApplicationCatalog::build` is a total function over source documents:

| Input | Result |
| --- | --- |
| Valid visible document | One `ApplicationEntry`, id = source id |
| `Hidden=true` or `NoDisplay=true` | Identity claimed, then silently skipped (normal deletion/visibility marker) |
| Parse failure | `InvalidDocument` diagnostic |
| Repeated source id | `DuplicateEntryId` diagnostic; first document claims the id even when hidden or invalid |
| Entry or source ceiling | `EntryLimitReached`/`SourceLimitReached` diagnostic; build stops |

Entries are ordered case-insensitively by display name, then id. The order is
stable for identical input. Diagnostics are bounded; overflow sets a
truncation flag instead of growing unbounded.

## Categories, search, and launch intents

`LauncherCategoryModel` maps XDG categories onto twelve fixed QindaQt groups
in a stable presentation order; unmapped entries land in Other. The mapping is
locale-independent by design.

`LauncherSearchRanker` matches the normalized (trimmed, whitespace-collapsed)
query case-insensitively and ranks by match quality — name prefix, name word
start, name substring, keyword prefix, keyword substring, generic name, then
comment — breaking ties by name, then id. Blank queries are a typed error, not
a request for the full catalog; callers wanting the default listing browse the
catalog or categories directly.

Activation resolves through `ApplicationCatalog::makeLaunchIntent`, which
returns a `LaunchIntent`: entry id, optional action id, and display values.
Keyboard and pointer activation share this one resolver. The intent carries no
command line and no execution path; starting a process is a later adapter
boundary, so a hostile document cannot reach execution logic through the
model.

## Pinned, recent, and presentation

`PinnedApplications` is an ordered identity list (ceiling 16) with explicit
pin/unpin/move outcomes. `RecentApplications` is a bounded (ceiling 8)
most-recently-used list where recording an existing id moves it to the front
and the oldest id is evicted. Neither model persists anything; a durable list
is a future Settings1-backed boundary that will consume these models.

`LauncherPresentationModel::build` projects a catalog, pinned, and recent
state into ordered sections and items:

- no catalog yet → `Loading`;
- catalog with diagnostics → `Degraded` (remaining valid entries still
  present);
- catalog without entries or diagnostics → `Empty`;
- otherwise → `Ready` with Pinned, Recent, and category sections; a valid
  query collapses the surface to one search-results section.

Focus order is exactly section order then item order (`itemAt`), and every
item carries display and accessible values (name, icon, a non-empty description)
so a QML adapter can render keyboard- and screen-reader-ready surfaces without
owning policy. Sections expose stable label/category identities rather than
hard-coded English; localization belongs to that adapter. A valid search always
publishes one SearchResults section, including the no-match state. Stale
pinned/recent ids that are no longer visible entries silently disappear from
the projection.

## Current boundary

This is the source/static L0 slice only. Installed-application scanning,
process execution, persistence, and the shell QML launcher applet are later
milestones; the launcher manifest in the applet catalog therefore still
resolves as `implementation-unavailable` (see
[Applet runtime](applet-runtime.md)). Hostile-fixture coverage lives in
`tests/shell/launcher` and covers malformed, duplicate, and hidden documents,
ranking ties and normalization, pinned/recent bounds, and the presentation
states.
