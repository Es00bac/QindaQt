# ADR-0126: Ignore user-override entries the schema cannot normalize

- **Status:** Accepted
- **Date:** 2026-09-10
- **Owners:** Settings service
- **Supersedes:** None
- **Superseded by:** None

## Context

`org.qindaqt.Settings1` composes one immutable profile-defaults document and
one optional user-overrides file. Both were loaded strictly: any entry the
active schema could not normalize — a key the schema does not define, or a
value outside its bounds — failed startup with `corrupt-user-overrides` and
process exit status 3, without mutating the file
([ADR-0012](0012-persist-notification-quieting-through-settings1.md)).

That strictness is right for the installed profile, which ships with the
package and is validated by the release gates. It is wrong for the user file.
The user file is shared by every build that runs against the same
`$XDG_CONFIG_HOME`: a development tree whose schema defines an extra key can
write that key through its own service, and the installed service then refuses
to start until someone edits the file by hand. Because the shell, the
compositor plugin, the appearance portal, the Settings application, and every
bundled application activate Settings1 on demand, one stray entry took the
whole desktop down: no wallpaper, no pinned dock entries, no appearance route.
On 2026-09-10 the key `services.terminalRestoreTabs`, written by a terminal
lane's tree, did exactly that on the maintainer's host.

## Decision

1. **The user file is loaded with a drop-invalid policy.** `SettingsSchema`
   gains `normalizedLayerDroppingInvalid`, `SettingsDocumentCodec::fromJson`
   and `SettingsCompatibilityLoader::load` accept a `DocumentValuePolicy`, and
   the resident service loads its user-overrides document with
   `DropInvalidValues`. Every entry the active schema normalizes is kept; every
   entry it cannot normalize is left out of the composed user layer, so the
   key resolves through the lower layers exactly as if the user had never set
   it.
2. **Ignored entries are reported, not hidden.** The start result carries the
   dropped entries as `ignoredUserOverrides`; the service process prints one
   `ignored-user-override: <file>: <key>: <reason>` line per entry to stderr,
   which the session journal keeps. Startup still does not rewrite the file.
   The entries disappear from disk only when the next committed user
   transaction persists the composed layer, which is the existing save path.
3. **Everything else stays strict.** Structural corruption (invalid JSON, a
   missing or wrong layer, a `values` member that is not an object), an
   unsupported schema version, a legacy document that fails migration, wire
   bounds, and the profile-defaults document all fail startup as before. The
   transaction path is untouched: an unknown key in a commit still rejects the
   whole transaction as `UnknownKey`
   ([settings service](../architecture/settings-service.md)).

## Consequences

- A newer or older build sharing the same user file can no longer stop the
  installed desktop from starting; the worst case is one setting reverting to
  its profile or system default until the user sets it again.
- Operators can see what was ignored and why in the journal without the
  service having to fail to say so.
- `SettingsServiceStartResult` grows one field; existing callers that only
  test `ok()` and `message` are unchanged.
- The lifecycle, schema, persistence, and migration suites gain typed coverage
  for both policies, including the negative cases that must stay fatal.
