# P1: Settings1 misclassifies an unknown-key commit as malformed

- **Timestamp:** 2026-08-27T11:45:31-06:00
- **Exact candidate:** `08c7156c578eaac21116498ed563828be4c1a625`
- **Severity:** P1 — blocks release/integration
- **Evidence boundary:** isolated `dbus-run-session`, temporary XDG config,
  exact Debug candidate executable; no user session or desktop

The normative Settings1 protocol requires `CommitUserTransaction` to
distinguish `UnknownKey` (ordinal 7) from `MalformedRequest` (ordinal 8). A
well-formed bounded transaction for syntactically valid `unknown.key`, sent
with the exact epoch and current revision, instead returned status 8:

```text
({'changedKeys': <@as []>,
  'message': <'invalid QVariant is not JSON null'>,
  'revisionAfter': <uint64 0>, 'revisionBefore': <uint64 0>,
  'sourceLayers': <@a{sv} {}>, 'status': <uint32 8>,
  'values': <@a{sv} {}>, ...},)
```

This request is not malformed: its outer envelope, operation field set, key,
kind, value, epoch, and base revision are all valid. It reaches model
validation. `SettingsRepository::commitUserOverrides()` maps the schema's
unknown-key rejection to `ValidationFailed`, then `currentAsResult()` inserts
`m_settings.value("unknown.key")`, an invalid QVariant, into `currentValues`
(`settings_repository.cpp:105-115` and `:52-67`). `SettingsObject` cannot
encode that invalid value and replaces the semantic result with a
`MalformedRequest` reply (`settings_object.cpp:270-283`). The declared
`UnknownKey` commit outcome is therefore unreachable through this path, and
the returned diagnostic describes an internal reply-construction failure
rather than the caller's semantic error.

This contradicts `docs/wiki/reference/settings1-v1.md:21-27` and the public
`SettingsWireStatus` contract. It is also missing from the service lifecycle
coverage: the repository test asserts only local `ValidationFailed` for an
unknown key and never checks the public D-Bus outcome.

Repair should detect unknown operation keys as a semantic `UnknownKey` result
before constructing authoritative value/source maps for keys that have no
authority, define the exact empty/per-key map rule for that outcome, and add a
private-D-Bus assertion for status, revisions, empty change set, no signal,
and no persistence/mutation. The reference and client validator must agree on
the map shape because the current prose says every commit outcome carries a
current value/source for every operated key, which is impossible for an
unknown key.
