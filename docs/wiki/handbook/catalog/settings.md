# Every Settings schema key and default

> Repository snapshot: `9728612046940b55d69f85c3811eb38a08a0963b`. This catalog records checked-in contracts and evidence at that commit; it does not claim new runtime verification. Canonical linked pages remain authoritative as the project changes.

All **48 keys** in active `data/settings/schema-v2.json` are listed below. JSON literals preserve type: for example, `"[]"` is a string, while `{}` is an object. An absent constraint means the schema adds no constraint beyond its declared type, not that all consuming applications accept arbitrary content. A schema key is a storage contract, not proof that its live platform behavior is implemented. See [Settings service](../../architecture/settings-service.md) and [Settings1](../../reference/settings1-v1.md) for resolution, persistence, transaction, and transport rules. The immutable v1 schema remains at `data/settings/schema-v1.json`; packaged profile defaults live in `data/settings/profile-defaults/qindaqt.json`.

## appearance

| Key | Type | Default | Schema constraints |
| --- | --- | --- | --- |
| `appearance.theme` | string | `"qinda-dark"` | `{"nonEmpty":true}` |
| `appearance.colorScheme` | string | `"system"` | `{"allowedValues":["system","light","dark"]}` |
| `appearance.wallpaper` | string | `""` | None beyond type |
| `appearance.wallpaperMode` | string | `"scaled"` | `{"allowedValues":["scaled","centered","tiled"]}` |
| `appearance.uiScale` | number | `1.0` | `{"minimum":0.5,"maximum":3.0}` |
| `appearance.accentColor` | string | `"#4f8cff"` | `{"nonEmpty":true}` |
| `appearance.blurEnabled` | boolean | `true` | None beyond type |
| `appearance.animationsEnabled` | boolean | `true` | None beyond type |
| `appearance.animationDurationMs` | integer | `180` | `{"minimum":0,"maximum":1000}` |
| `appearance.windowButtonStyle` | string | `"theme"` | `{"allowedValues":["theme","traffic-lights","flat","glyph"]}` |
| `appearance.windowButtonSide` | string | `"theme"` | `{"allowedValues":["theme","left","right"]}` |
| `appearance.windowButtons` | string | `"all"` | `{"allowedValues":["all","minimize-close","close"]}` |
| `appearance.windowTitleAlignment` | string | `"center"` | `{"allowedValues":["center","left"]}` |
| `appearance.containerButtonStyle` | string | `"theme"` | `{"allowedValues":["theme","traffic-lights","flat"]}` |
| `appearance.containerButtonSide` | string | `"theme"` | `{"allowedValues":["theme","left","right"]}` |
| `appearance.containerTabOrder` | string | `"theme"` | `{"allowedValues":["theme","left-to-right","right-to-left"]}` |
| `appearance.containerButtonGlyphs` | string | `"theme"` | `{"allowedValues":["theme","always","hover"]}` |

## fonts

| Key | Type | Default | Schema constraints |
| --- | --- | --- | --- |
| `fonts.family` | string | `"Noto Sans"` | `{"nonEmpty":true}` |
| `fonts.monospaceFamily` | string | `"Noto Sans Mono"` | `{"nonEmpty":true}` |
| `fonts.pointSize` | number | `10.0` | `{"minimum":6.0,"maximum":36.0}` |
| `fonts.antialiasing` | boolean | `true` | None beyond type |
| `fonts.hinting` | string | `"slight"` | `{"allowedValues":["none","slight","medium","full"]}` |
| `fonts.subpixelOrder` | string | `"rgb"` | `{"allowedValues":["none","rgb","bgr","vrgb","vbgr"]}` |

## displays

| Key | Type | Default | Schema constraints |
| --- | --- | --- | --- |
| `displays.fractionalScaling` | boolean | `true` | None beyond type |
| `displays.primaryOutput` | string | `""` | None beyond type |
| `displays.configuration` | object | `{}` | None beyond type |
| `displays.hdrPolicy` | string | `"automatic"` | `{"allowedValues":["off","automatic","on"]}` |
| `displays.colorAssignments` | object | `{}` | None beyond type |

## input

| Key | Type | Default | Schema constraints |
| --- | --- | --- | --- |
| `input.pointerAcceleration` | number | `0.0` | `{"minimum":-1.0,"maximum":1.0}` |
| `input.naturalScroll` | boolean | `false` | None beyond type |
| `input.keyboardRepeatDelayMs` | integer | `500` | `{"minimum":100,"maximum":2000}` |
| `input.keyboardRepeatRate` | integer | `30` | `{"minimum":1,"maximum":100}` |
| `input.tapToClick` | boolean | `true` | None beyond type |

## panels

| Key | Type | Default | Schema constraints |
| --- | --- | --- | --- |
| `panels.layoutProfile` | string | `"qindaqt"` | `{"nonEmpty":true}` |
| `panels.autoHideDelayMs` | integer | `250` | `{"minimum":0,"maximum":5000}` |
| `panels.configuration` | object | `{}` | None beyond type |

## window-management

| Key | Type | Default | Schema constraints |
| --- | --- | --- | --- |
| `windowManagement.dockingModifier` | string | `"super"` | `{"allowedValues":["super","alt","control","disabled"]}` |
| `windowManagement.snapDistance` | integer | `12` | `{"minimum":0,"maximum":64}` |
| `windowManagement.focusPolicy` | string | `"click"` | `{"allowedValues":["click","focus-follows-mouse","focus-under-mouse"]}` |
| `windowManagement.sessionRestore` | boolean | `true` | None beyond type |
| `windowManagement.closeContainerPolicy` | string | `"ask"` | `{"allowedValues":["ask","close-all","ungroup"]}` |

## accessibility

| Key | Type | Default | Schema constraints |
| --- | --- | --- | --- |
| `accessibility.highContrast` | boolean | `false` | None beyond type |
| `accessibility.reducedMotion` | boolean | `false` | None beyond type |
| `accessibility.reducedTransparency` | boolean | `false` | None beyond type |
| `accessibility.textScale` | number | `1.0` | `{"minimum":0.5,"maximum":3.0}` |
| `accessibility.screenReader` | boolean | `false` | None beyond type |

## services

| Key | Type | Default | Schema constraints |
| --- | --- | --- | --- |
| `services.clipboardHistory` | boolean | `false` | None beyond type |
| `services.notifications` | boolean | `true` | None beyond type |
| `services.doNotDisturb` | boolean | `false` | None beyond type |
| `services.bluetooth` | boolean | `true` | None beyond type |
| `services.metricsHistory` | boolean | `false` | None beyond type |
| `services.xwaylandOnDemand` | boolean | `true` | None beyond type |
| `services.terminalProfiles` | string | `"[]"` | None beyond type |
| `services.terminalDefaultProfile` | string | `"builtin-default"` | None beyond type |
| `services.terminalRestoreWindows` | boolean | `false` | None beyond type |
| `services.textEditorRestoreDocuments` | boolean | `false` | None beyond type |

## Packaged profile overrides

These values are an additional layer, not replacements for the schema defaults above.

```json
{
  "schemaVersion": 2,
  "layer": "profile-defaults",
  "values": {
    "appearance.animationDurationMs": 160,
    "appearance.blurEnabled": true,
    "appearance.theme": "qinda-dark",
    "panels.layoutProfile": "qindaqt",
    "windowManagement.dockingModifier": "super"
  }
}
```

