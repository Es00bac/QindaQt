# Layout profile schema v1

This page records the profile format accepted by the current
`src/profiles` loader. The broader interaction contract in
[Layout profiles](../shell/layout-profiles.md) includes planned fields that are
not persisted by schema v1 yet.

## Root object

| Field | Type | Current rule |
| --- | --- | --- |
| `schemaVersion` | integer | Required and exactly `1` |
| `id` | string | Required, non-empty, and unique within a catalog |
| `name` | string | Required and non-empty |
| `description` | string | Optional user-facing description |
| `defaultTheme` | string | Optional theme ID; defaults to `qinda-dark` |
| `workflow` | object | Optional workflow hints described below |
| `panels` | array | At least one valid panel; panel IDs must be unique |

The current loader ignores and does not preserve unknown fields. Do not rely on
an unknown field affecting the shell until it appears on this page and has
validation coverage.

## Workflow object

All workflow fields are strings except `globalMenu`. Defaults are `compact`
overview, `static` workspaces, `shelf` launcher, `global` menu, `grouped` task
list, and an enabled global menu. These values are presentation hints in the
foundation preview; their complete behavior will be specified as controllers
land.

## Panel object

| Field | Type | Accepted values |
| --- | --- | --- |
| `id` | string | Required, non-empty |
| `output` | string | Logical output selector; defaults to `*` |
| `edge` | string | `top`, `bottom`, `left`, or `right` |
| `layer` | string | `below`, `normal`, `above`, or `overlay` |
| `hideMode` | string | `never`, `intelligent`, `dodge-active`, `dodge-all`, `maximized`, or `always` |
| `alignment` | string | `start`, `center`, `end`, or `fill` |
| `rows` | integer | `1` through `4` |
| `thickness` | integer | `20` through `192` logical pixels |
| `length` | number | `0.1` through `1.0` of the selected output edge |
| `applets` | array | Ordered applet instances with unique IDs within the panel |

An applet requires non-empty `id` and `plugin` strings. Its optional `settings`
object is passed through as declarative profile data; it never grants runtime
capabilities. The foundation panel renderer recognizes `settings.zone` values
`start`, `center`, and `end`, plus the visual `settings.bare` boolean. Applet
plugins will own their remaining settings schemas as their runtimes land.

Margins, opacity, exclusive zones, richer monitor matching, shortcuts, and
user-derived profile metadata remain planned extensions. Adding them requires
loader tests, migration policy, and a same-change update to this page.
