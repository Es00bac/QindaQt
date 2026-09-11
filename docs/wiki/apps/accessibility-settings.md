# QindaQt Settings — Accessibility route

`qindaqt-settings --page accessibility` edits the four Settings1 schema v2
accessibility keys that already have live consumers:

| Key | Type | Default | Consumers |
| --- | --- | --- | --- |
| `accessibility.highContrast` | Boolean | `false` | Qt platform theme, shell surfaces, QST-1 token deriver |
| `accessibility.reducedMotion` | Boolean | `false` | Qt platform theme, shell panel visibility, QST-1 motion tokens |
| `accessibility.reducedTransparency` | Boolean | `false` | Qt platform theme, shell panel material, QST-1 |
| `accessibility.textScale` | number, 0.5–3.0 | `1.0` | Qt platform theme fonts, shell text, QST-1 type scale |

`accessibility.screenReader` is defined by the schema but **reserved**: no
consumer reads it yet, so the route neither scopes, reads, nor writes it. The
boundary scan rejects any route source that names the key. The route decision
is [ADR-0128](../adr/0128-accessibility-settings-route.md); the navigation
shell that hosts it is the [Settings Center](settings-center.md).

## Truth and mutation

The Settings executable owns one independent `QtSettingsTransport` and a
`SettingsClient` scoped to exactly the four keys above, and hands the page only
the route model (`AccessibilitySettingsModel`). The model keeps the last
confirmed values and one draft. A snapshot must carry every scoped key as a
Boolean or an in-range number (integral wire numbers are accepted for the
scale); anything else makes the route Unavailable and revokes edit and apply
admission while preserving the draft.

Edits accumulate in the draft; nothing is written until **Apply**. Because the
public client commits one key per transaction, Apply writes the changed keys
one at a time in schema order (`highContrast`, `reducedMotion`,
`reducedTransparency`, `textScale`), issuing each next key only from the fresh
post-commit snapshot so every write carries the current base revision. The
route never claims one atomic transaction. **Revert** returns the draft to the
confirmed values and is refused while a commit is in flight.

- A `Conflict` reply whose current value already equals the intended value
  counts as applied; any other conflict stops the sequence, keeps the draft,
  reloads current values, and requires an explicit Apply to restate the
  remaining choice.
- Validation, read-only, and unknown-key rejections stop the sequence and
  stay visible until the next explicit Apply or Revert.
- A timeout, owner replacement, or bus loss during a write is reported as
  uncertain and never replayed; **Retry** only refreshes authority.
- An out-of-range scale (`< 0.5` or `> 3.0`) is refused by the model with a
  visible error; the slider itself is bounded to the schema range.

## Page

The page follows the product direction of short labels and minimal prose.
Three `Switch` rows (High contrast, Reduce motion, Reduce transparency) and a
`Slider` for Text scale carry their one-sentence explanations as
`Accessible.description` and as attached `ToolTip` text rather than as visible
paragraphs. A live sample label scales its point size with the slider so the
effect is visible before Apply. The action row offers Apply, Revert, and — only
while the route is Unavailable — Retry; a `DegradedNotice` with the same Retry
carries the unavailable reason. The first focus target is the High contrast
switch, or Retry when editing is not admitted. The route is appended last in
the registry, so it has no `Ctrl+digit` shortcut; keyboard users reach it from
the sidebar or compact tab list.

## Verification

```sh
ctest --test-dir build/dev --output-on-failure --no-tests=error \
  -R '^qindaqt\.settings-accessibility-'
```

- `settings-accessibility-model` — exact four-key scoping (reserved key
  excluded), integral scale decode, setter gating and Revert, per-key commit
  order from fresh snapshots, malformed-snapshot fail-closed, conflict stop and
  explicit re-Apply, uncertain no-replay, and replacement-abort without replay,
  all against an injected fake transport;
- `settings-accessibility-page` — warning-fatal offscreen wide/compact
  construction, accessible roles/names/descriptions, tooltip-carried help,
  draft/apply/revert wiring, live sample scaling, saving fence, and the
  Unavailable notice with Retry, against a stub model;
- `settings-accessibility-boundary` / `-boundary-poison` — public-client-only
  include allow-list, consumer-reach and D-Bus rejection, and the reserved
  screen-reader key guard;
- `settings-accessibility-installed-route` — relocated stage with the module
  withheld (exit 3) and restored (resident) under poisoned buses.

The Settings Center rows add the registry index (10), root construction, and
the installed package poison for the Accessibility module. This slice does not
claim live AT-SPI traversal, a screen-reader toggle, or per-application
overrides.
