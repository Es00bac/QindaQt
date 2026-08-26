# Layout profile schema v1

This page is the authoritative contract implemented by `src/profiles`. The
broader interaction contract in [Layout profiles](../shell/layout-profiles.md)
includes planned fields that schema v1 does not persist yet.

## Document and validation contract

A profile is an RFC 8259 UTF-8 JSON object. Loading is strict and atomic:

- malformed UTF-8, unescaped control characters, unknown escape sequences,
  invalid surrogate pairs, non-standard number spellings, duplicate object
  keys, trailing content, and more than 64 nested JSON containers are rejected;
- strings, booleans, arrays, objects, and numbers are never coerced to another
  field type;
- optional defaults apply only when a field is absent. Explicit `null` or a
  wrong-typed value is an error;
- integer fields accept any JSON number that is mathematically integral, such
  as `1`, `1.0`, or `1e0`, and reject fractional or unrepresentable values; and
- unknown fields are validated as JSON syntax but otherwise ignored and are
  not preserved by a model round trip.

Validation reports the first deterministic failure as a structured
`ProfileError`: a stable `ProfileErrorCode`, source `origin`, RFC 6901 `path`,
panel and applet IDs when known, a message, and a zero-based byte offset for
syntax failures. A failed load returns no partially populated profile. File
read, syntax, root type, field type, enum, range, identity, and settings-value
failures remain distinguishable to callers.

`ProfileValidator::validate()` applies the same semantic rules to programmatic
`LayoutProfile` candidates. `validatePanelLayout()` is intentionally narrower:
it validates only one panel's identity, selector, enum, and geometry values and
does not make the layout module an owner of applet identity or settings.

## Root object

| Field | Type | Rule and absent-field default |
| --- | --- | --- |
| `schemaVersion` | integer | Required and exactly `1` |
| `id` | string | Required profile ID; non-empty with no surrounding whitespace; catalog-unique |
| `name` | string | Required and not blank |
| `description` | string | Optional; defaults to an empty string |
| `defaultTheme` | string | Optional theme ID; defaults to `qinda-dark`; non-empty with no surrounding whitespace |
| `workflow` | object | Optional; absent subfields use the defaults below |
| `panels` | array | Required and non-empty; panel IDs are profile-global and unique |

## Workflow object

| Field | Type | Absent-field default |
| --- | --- | --- |
| `overview` | string | `compact` |
| `workspacePolicy` | string | `static` |
| `launcher` | string | `shelf` |
| `menu` | string | `global` |
| `taskList` | string | `grouped` |
| `globalMenu` | boolean | `true` |

Workflow strings must not be blank. They are presentation hints in the
foundation implementation; their complete behavior will be specified as the
corresponding controllers land.

## Panel object

| Field | Type | Rule and absent-field default |
| --- | --- | --- |
| `id` | string | Required panel ID; non-empty with no surrounding whitespace; unique in the profile |
| `output` | string | Logical output selector; defaults to `*`; non-empty with no surrounding whitespace |
| `edge` | string | `top`, `bottom`, `left`, or `right`; defaults to `top` |
| `layer` | string | `below`, `normal`, `above`, or `overlay`; defaults to `above` |
| `hideMode` | string | `never`, `intelligent`, `dodge-active`, `dodge-all`, `maximized`, or `always`; defaults to `never` |
| `alignment` | string | `start`, `center`, `end`, or `fill`; defaults to `fill` |
| `rows` | integer | Inclusive range `1` through `4`; defaults to `1` |
| `thickness` | integer | Inclusive range `20` through `192` logical pixels; defaults to `32` |
| `length` | number | Finite inclusive range `0.1` through `1.0`; defaults to `1.0` |
| `applets` | array | Optional ordered applet instances; defaults to an empty array |

Enum spellings are canonical and case-sensitive in JSON. Public enum helpers
may accept case-insensitive user input, but the persisted profile reader does
not use that behavior to normalize a document.

## Applet object and settings

| Field | Type | Rule and absent-field default |
| --- | --- | --- |
| `id` | string | Required instance ID; non-empty with no surrounding whitespace; unique across every panel in the profile |
| `plugin` | string | Required implementation ID; non-empty with no surrounding whitespace |
| `settings` | object | Optional JSON-native settings; defaults to an empty object |

Profile-global applet identity is the accepted decision in
[ADR-0006](../adr/0006-profile-global-applet-identity.md). An applet move keeps
its ID, while duplication allocates a new one. Expanding a wildcard panel onto
multiple outputs does not duplicate the persistent applet identity.

JSON-loaded settings preserve null, boolean, signed 64-bit integer, finite
number, string, array, and object values. They never grant runtime capabilities;
an applet plugin owns the meaning and finer schema of its settings.

Programmatic candidates use a `QVariantMap` and are checked before
serialization. Accepted values are `std::nullptr_t`, booleans, `int`, `uint`,
`qint64`, `quint64` no greater than `qint64` maximum, finite `float`/`double`,
`QString`, `QStringList`, recursively valid `QVariantList`, `QVariantMap`, or
`QVariantHash`, and recursively valid `QJsonValue`, `QJsonArray`, or
`QJsonObject` values. Every programmatic model string, settings string, and
settings object key must contain well-formed UTF-16. The finite-number and
well-formed-string rules also apply inside Qt JSON wrapper types.

An invalid `QVariant`, undefined `QJsonValue`, non-finite number, ill-formed
string or object key, oversized unsigned integer, byte array, URL, UUID, CBOR
value, custom type, or any value requiring Qt's lossy coercion is rejected.
Programmatic settings nesting is limited to 64 containers, and representable
object keys in error paths use RFC 6901 escaping.

The foundation panel renderer currently recognizes `settings.zone` values
`start`, `center`, and `end`, plus the visual `settings.bare` boolean. Remaining
plugin-specific constraints belong to applet manifests and runtimes.

Margins, opacity, exclusive zones, richer monitor matching, shortcuts, and
user-derived profile metadata remain planned extensions. Adding them requires
loader tests, migration policy, and a same-change update to this page.
