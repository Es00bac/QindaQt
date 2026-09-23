# Every Settings1 schema key and default

> Source snapshot: integrated Settings route registry and `data/settings/schema-v2.json` at commit `452a5697` (2026-09-23). This is a source-contract inventory, not physical hardware qualification.

All **85 keys** in active schema v2 are listed below. The immutable v1 schema remains at `data/settings/schema-v1.json`; packaged defaults are in `data/settings/profile-defaults/qindaqt.json`. JSON literals preserve type: `"[]"` is a string, while `[]` is a string-list. See [Settings service](../../architecture/settings-service.md) and [Settings1](../../reference/settings1-v1.md) for persistence and transport rules.

**Active route** means a first-party Settings control and a documented consumer or authority path exist; it does not assert a live physical test. **Active consumer; editor gap** means the value is applied but this base has no verified settings editor. **App-owned** means another named app/shell surface owns the interaction. **Legacy/superseded** remains in schema for compatibility while another authority owns current behavior. **Reserved** is intentionally hidden pending a real consumer. **Missing consumer** has no verified production consumer in this repository; an external package may still use it. Storage alone never proves an effective control.


## appearance

| Key | Type | Default | Schema constraints | Disposition / evidence |
| --- | --- | --- | --- | --- |
| `appearance.theme` | string | `"qinda-dark"` | `{"nonEmpty":true}` | **Active route** — Appearance owns the visible or confirmed preference boundary. |
| `appearance.colorScheme` | string | `"system"` | `{"allowedValues":["system","light","dark"]}` | **Active route** — Appearance owns the visible or confirmed preference boundary. |
| `appearance.wallpaper` | string | `""` | None beyond type | **Active route** — Appearance owns the visible or confirmed preference boundary. |
| `appearance.wallpaperMode` | string | `"scaled"` | `{"allowedValues":["scaled","centered","tiled"]}` | **Active route** — Appearance owns the visible or confirmed preference boundary. |
| `appearance.uiScale` | number | `1.0` | `{"minimum":0.5,"maximum":3.0}` | **Missing consumer** — Appearance round-trips the key, but no visible scale editor or runtime consumer was found; Display1 output scale is separate. |
| `appearance.accentColor` | string | `"#4f8cff"` | `{"nonEmpty":true}` | **Missing consumer** — No verified production consumer; theme/QST policy is not proof this stored key is applied. |
| `appearance.blurEnabled` | boolean | `true` | None beyond type | **Missing consumer** — No verified production consumer. |
| `appearance.animationsEnabled` | boolean | `true` | None beyond type | **Missing consumer** — No verified production consumer. |
| `appearance.animationDurationMs` | integer | `180` | `{"minimum":0,"maximum":1000}` | **Missing consumer** — No verified production consumer. |
| `appearance.windowButtonStyle` | string | `"theme"` | `{"allowedValues":["theme","traffic-lights","flat","glyph"]}` | **Active route** — Appearance owns the visible or confirmed preference boundary. |
| `appearance.windowButtonSide` | string | `"theme"` | `{"allowedValues":["theme","left","right"]}` | **Active route** — Appearance owns the visible or confirmed preference boundary. |
| `appearance.windowButtons` | string | `"all"` | `{"allowedValues":["all","minimize-close","close"]}` | **Active route** — Appearance owns the visible or confirmed preference boundary. |
| `appearance.windowTitleAlignment` | string | `"center"` | `{"allowedValues":["center","left"]}` | **Active route** — Appearance owns the visible or confirmed preference boundary. |
| `appearance.containerButtonStyle` | string | `"theme"` | `{"allowedValues":["theme","traffic-lights","flat"]}` | **Active route** — Appearance owns the visible or confirmed preference boundary. |
| `appearance.containerButtonSide` | string | `"theme"` | `{"allowedValues":["theme","left","right"]}` | **Active route** — Appearance owns the visible or confirmed preference boundary. |
| `appearance.containerTabOrder` | string | `"theme"` | `{"allowedValues":["theme","left-to-right","right-to-left"]}` | **Active route** — Appearance owns the visible or confirmed preference boundary. |
| `appearance.containerButtonGlyphs` | string | `"theme"` | `{"allowedValues":["theme","always","hover"]}` | **Active route** — Appearance owns the visible or confirmed preference boundary. |
| `appearance.windowDecoration` | string | `"theme"` | `{"nonEmpty":true}` | **Active route** — Appearance owns the visible or confirmed preference boundary. |
| `appearance.containerDecoration` | string | `"theme"` | `{"nonEmpty":true}` | **Active route** — Appearance owns the visible or confirmed preference boundary. |

## fonts

| Key | Type | Default | Schema constraints | Disposition / evidence |
| --- | --- | --- | --- | --- |
| `fonts.family` | string | `"Noto Sans"` | `{"nonEmpty":true}` | **Active route** — Appearance owns the visible or confirmed preference boundary. |
| `fonts.monospaceFamily` | string | `"Noto Sans Mono"` | `{"nonEmpty":true}` | **Active route** — Appearance owns the visible or confirmed preference boundary. |
| `fonts.pointSize` | number | `10.0` | `{"minimum":6.0,"maximum":36.0}` | **Active route** — Appearance owns the visible or confirmed preference boundary. |
| `fonts.antialiasing` | boolean | `true` | None beyond type | **Active route** — Appearance owns the visible or confirmed preference boundary. |
| `fonts.hinting` | string | `"slight"` | `{"allowedValues":["none","slight","medium","full"]}` | **Active route** — Appearance owns the visible or confirmed preference boundary. |
| `fonts.subpixelOrder` | string | `"rgb"` | `{"allowedValues":["none","rgb","bgr","vrgb","vbgr"]}` | **Active route** — Appearance owns the visible or confirmed preference boundary. |

## displays

| Key | Type | Default | Schema constraints | Disposition / evidence |
| --- | --- | --- | --- | --- |
| `displays.fractionalScaling` | boolean | `true` | None beyond type | **Legacy/superseded** — Live display topology and scale use Display1, not this Settings1 field. |
| `displays.primaryOutput` | string | `""` | None beyond type | **Legacy/superseded** — Live display topology and scale use Display1, not this Settings1 field. |
| `displays.configuration` | object | `{}` | None beyond type | **Legacy/superseded** — Live display topology and scale use Display1, not this Settings1 field. |
| `displays.hdrPolicy` | string | `"automatic"` | `{"allowedValues":["off","automatic","on"]}` | **Legacy/superseded** — Legacy Settings1 field; live HDR control is not established. |
| `displays.colorAssignments` | object | `{}` | None beyond type | **Active route** — Color route and assignment store; compositor apply/readback is documented separately. |

## input

| Key | Type | Default | Schema constraints | Disposition / evidence |
| --- | --- | --- | --- | --- |
| `input.pointerAcceleration` | number | `0.0` | `{"minimum":-1.0,"maximum":1.0}` | **Legacy/superseded** — Input uses native KWin per-device settings (ADR-0134). |
| `input.naturalScroll` | boolean | `false` | None beyond type | **Legacy/superseded** — Input uses native KWin per-device settings (ADR-0134). |
| `input.keyboardRepeatDelayMs` | integer | `500` | `{"minimum":100,"maximum":2000}` | **Legacy/superseded** — Input uses native KWin per-device settings (ADR-0134). |
| `input.keyboardRepeatRate` | integer | `30` | `{"minimum":1,"maximum":100}` | **Legacy/superseded** — Input uses native KWin per-device settings (ADR-0134). |
| `input.tapToClick` | boolean | `true` | None beyond type | **Legacy/superseded** — Input uses native KWin per-device settings (ADR-0134). |
| `input.tabletMappings` | object | `{}` | None beyond type | **Active route** — Input tablet mapping store and route. |
| `input.touch.enabled` | boolean | `true` | None beyond type | **Active route** — Input owns the visible or confirmed preference boundary. |
| `input.touch.longPressMs` | integer | `500` | `{"minimum":200,"maximum":1500}` | **Active route** — Input owns the visible or confirmed preference boundary. |
| `input.touch.mode` | string | `"auto"` | `{"allowedValues":["auto","on","off"]}` | **Reserved** — No runtime consumer; Input route intentionally omits it. |
| `input.touch.onScreenKeyboard` | string | `"auto"` | `{"allowedValues":["auto","off"]}` | **Active route** — Input owns the visible or confirmed preference boundary. |
| `input.touch.edgeLeft` | string | `"overview"` | `{"allowedValues":["none","overview","notifications","task-switcher"]}` | **Active route** — Input owns the visible or confirmed preference boundary. |
| `input.touch.edgeTop` | string | `"notifications"` | `{"allowedValues":["none","overview","notifications","task-switcher"]}` | **Active route** — Input owns the visible or confirmed preference boundary. |
| `input.touch.edgeRight` | string | `"none"` | `{"allowedValues":["none","overview","notifications","task-switcher"]}` | **Active route** — Input owns the visible or confirmed preference boundary. |
| `input.touch.edgeBottom` | string | `"task-switcher"` | `{"allowedValues":["none","overview","notifications","task-switcher"]}` | **Active route** — Input owns the visible or confirmed preference boundary. |

## panels

| Key | Type | Default | Schema constraints | Disposition / evidence |
| --- | --- | --- | --- | --- |
| `panels.launcherPinned` | string-list | `[]` | None beyond type | **App-owned** — Launcher owns pinned-item editing. |
| `panels.launcherRecent` | string-list | `[]` | None beyond type | **App-owned** — Launcher owns recent-item history. |
| `panels.layoutProfile` | string | `"qindaqt"` | `{"nonEmpty":true}` | **Active route** — Customize owns the visible or confirmed preference boundary. |
| `panels.autoHideDelayMs` | integer | `250` | `{"minimum":0,"maximum":5000}` | **Active consumer; editor gap** — Shell PanelVisibilityRuntime consumes it; no owned editor was verified on this base. |
| `panels.configuration` | object | `{}` | None beyond type | **App-owned** — Panel quick configuration and task-order persistence; edit through Customize/panel controls. |

## window-management

| Key | Type | Default | Schema constraints | Disposition / evidence |
| --- | --- | --- | --- | --- |
| `windowManagement.dockingModifier` | string | `"super"` | `{"allowedValues":["super","alt","control","disabled"]}` | **Active route** — Windows & workspaces owns the visible or confirmed preference boundary. |
| `windowManagement.snapDistance` | integer | `12` | `{"minimum":0,"maximum":64}` | **Active route** — Windows & workspaces owns the visible or confirmed preference boundary. |
| `windowManagement.focusPolicy` | string | `"click"` | `{"allowedValues":["click","focus-follows-mouse","focus-under-mouse"]}` | **Active route** — Windows & workspaces owns the visible or confirmed preference boundary. |
| `windowManagement.sessionRestore` | boolean | `true` | None beyond type | **Reserved** — Written through, but no session-restore behavior consumes it; Windows route hides it. |
| `windowManagement.closeContainerPolicy` | string | `"ask"` | `{"allowedValues":["ask","close-all","ungroup"]}` | **Active route** — Windows & workspaces owns the visible or confirmed preference boundary. |

## accessibility

| Key | Type | Default | Schema constraints | Disposition / evidence |
| --- | --- | --- | --- | --- |
| `accessibility.highContrast` | boolean | `false` | None beyond type | **Active route** — Accessibility owns the visible or confirmed preference boundary. |
| `accessibility.reducedMotion` | boolean | `false` | None beyond type | **Active route** — Accessibility owns the visible or confirmed preference boundary. |
| `accessibility.reducedTransparency` | boolean | `false` | None beyond type | **Active route** — Accessibility owns the visible or confirmed preference boundary. |
| `accessibility.textScale` | number | `1.0` | `{"minimum":0.5,"maximum":3.0}` | **Active route** — Accessibility owns the visible or confirmed preference boundary. |
| `accessibility.screenReader` | boolean | `false` | None beyond type | **Reserved** — No provider/session integration; Accessibility route intentionally hides it. |

## services

| Key | Type | Default | Schema constraints | Disposition / evidence |
| --- | --- | --- | --- | --- |
| `services.clipboardHistory` | boolean | `false` | None beyond type | **Active route** — route-specific service owns the visible or confirmed preference boundary. |
| `services.voiceInput` | boolean | `false` | None beyond type | **Active route** — Voice route preference gates QindaQt provider use; independent providers remain separate. |
| `services.voicePanelTranscript` | boolean | `true` | None beyond type | **Active route** — Voice route and shell applet transcript visibility. |
| `services.notifications` | boolean | `true` | None beyond type | **Missing consumer** — No verified consumer of this particular master toggle; notification service presence is not proof. |
| `services.doNotDisturb` | boolean | `false` | None beyond type | **Active route** — Notifications route and shell interruption policy. |
| `services.obsWebSocketPort` | integer | `4455` | `{"minimum":1,"maximum":65535}` | **Active route** — Streaming route and confirmed streaming-preferences/login consumer. |
| `services.obsAutoConnect` | boolean | `true` | None beyond type | **Active route** — Streaming route and confirmed auto-connect consumer. |
| `services.obsStartAtLogin` | boolean | `false` | None beyond type | **Active route** — Streaming route and XDG login-entry policy. |
| `services.doNotDisturbSchedule` | boolean | `false` | None beyond type | **Active route** — Notifications Quiet Hours route and shell interruption policy. |
| `services.doNotDisturbStartMinutes` | integer | `1320` | `{"minimum":0,"maximum":1439}` | **Active route** — Notifications Quiet Hours route and shell interruption policy. |
| `services.doNotDisturbEndMinutes` | integer | `420` | `{"minimum":0,"maximum":1439}` | **Active route** — Notifications Quiet Hours route and shell interruption policy. |
| `services.bluetooth` | boolean | `true` | None beyond type | **Missing consumer** — No verified consumer of this Settings1 key; Bluetooth adapter authority is separate. |
| `services.metricsHistory` | boolean | `false` | None beyond type | **Missing consumer** — No verified production consumer. |
| `services.xwaylandOnDemand` | boolean | `true` | None beyond type | **Missing consumer** — No verified production consumer. |
| `services.terminalProfiles` | string | `"[]"` | None beyond type | **Missing consumer** — External QQ_Term contract is unverified from this repository; do not advertise a desktop control. |
| `services.terminalDefaultProfile` | string | `"builtin-default"` | None beyond type | **Missing consumer** — External QQ_Term contract is unverified from this repository. |
| `services.terminalRestoreWindows` | boolean | `false` | None beyond type | **Missing consumer** — External QQ_Term contract is unverified from this repository. |
| `services.calendarDefaultView` | string | `"month"` | None beyond type | **App-owned** — Calendar preferences own this choice. |
| `services.calendarWeekStart` | string | `"locale"` | None beyond type | **Active route** — Date & time route and Calendar preferences. |
| `services.calendarDefaultCalendar` | string | `"personal"` | None beyond type | **App-owned** — Calendar preferences own this choice. |
| `services.textEditorRestoreDocuments` | boolean | `false` | None beyond type | **App-owned** — Text Editor restore policy owns the preference. |

## shell

| Key | Type | Default | Schema constraints | Disposition / evidence |
| --- | --- | --- | --- | --- |
| `shell.shortcutNoteDismissed` | boolean | `false` | None beyond type | **App-owned** — Shell shortcut-help dismissal state, not a general Settings control. |
| `shell.customization.chord` | string | `"meta-right"` | `{"allowedValues":["meta-right","meta-alt-right"]}` | **Active consumer; editor gap** — LiveCustomizationController consumes it; no owned editor was verified on this base. |

## power

| Key | Type | Default | Schema constraints | Disposition / evidence |
| --- | --- | --- | --- | --- |
| `power.idleDisplayOffMinutes` | integer | `10` | `{"minimum":-1,"maximum":240}` | **Active route** — Power / Screen saver owns the visible or confirmed preference boundary. |
| `power.screensaver` | string | `"none"` | None beyond type | **Active route** — Power / Screen saver owns the visible or confirmed preference boundary. |
| `power.screensaverMinutes` | integer | `5` | `{"minimum":1,"maximum":240}` | **Active route** — Power / Screen saver owns the visible or confirmed preference boundary. |

## Packaged profile overrides

These are an additional layer, not replacements for schema defaults. The checked-in profile file currently overrides appearance animation duration, blur, theme, panel layout profile, and docking modifier. A profile default for a missing-consumer key does not make that key an applied desktop behavior.

For native settings outside Settings1—KWin input, Display1 topology, SDDM login screen, XDG MIME defaults, OBS, and service-owned device settings—see the [21-route completeness inventory](../../reference/settings-completeness.md).
