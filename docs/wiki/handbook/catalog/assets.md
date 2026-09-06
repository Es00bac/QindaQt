# Packaged profiles, themes, applets, and policy

> Repository snapshot: `9728612046940b55d69f85c3811eb38a08a0963b`. This catalog records checked-in contracts and evidence at that commit; it does not claim new runtime verification. Canonical linked pages remain authoritative as the project changes.

Profiles select workflow and panel composition; themes select appearance; applet manifests declare interfaces and requested authority. Presence in data does not establish a compiled implementation or platform qualification. Read [Applet runtime](../../shell/applet-runtime.md) before interpreting manifest or profile entries as live functionality.

## Layout profiles (10)

### gnome-inspired — Overview

Source: `data/profiles/gnome-inspired.json`. A top bar, searchable overview, dash, and dynamic workspaces.

Default theme: `qinda-dark`. Workflow: `{"overview":"activities","workspacePolicy":"dynamic","launcher":"overview-dash","menu":"application-title","taskList":"overview-only","globalMenu":false}`.

| Panel | Geometry and policy | Ordered applet instances |
| --- | --- | --- |
| top-bar | edge=`"top"`; layer=`"above"`; hideMode=`"never"`; alignment=`"fill"`; rows=`1`; thickness=`32`; length=`1.0` | `activities` → `overview-trigger` (`{}`); `active-app` → `active-application` (`{}`); `clock` → `clock` (`{}`); `clipboard` → `clipboard` (`{}`); `status-notifier` → `status-notifier` (`{}`); `notifications` → `notification-center` (`{}`); `status` → `system-status` (`{}`); `applications` → `launcher` (`{}`) |

### macos-inspired — Menu and Dock

Source: `data/profiles/macos-inspired.json`. A global top menu with a centered intelligent dock.

Default theme: `qinda-macos`. Workflow: `{"overview":"expose","workspacePolicy":"dynamic","launcher":"center-dock","menu":"global","taskList":"dock-indicators","globalMenu":true}`.

| Panel | Geometry and policy | Ordered applet instances |
| --- | --- | --- |
| global-bar | edge=`"top"`; layer=`"above"`; hideMode=`"never"`; alignment=`"fill"`; rows=`1`; thickness=`29`; length=`1.0` | `system-menu` → `system-menu` (`{"zone":"start","bare":true}`); `menu` → `global-menu` (`{"zone":"start","bare":true}`); `status` → `system-status` (`{"zone":"end","bare":true}`); `clipboard` → `clipboard` (`{"zone":"end","bare":true}`); `status-notifier` → `status-notifier` (`{"zone":"end","bare":true}`); `notifications` → `notification-center` (`{"zone":"end","bare":true}`); `clock` → `clock` (`{"zone":"end","bare":true}`) |
| dock | edge=`"bottom"`; layer=`"overlay"`; hideMode=`"intelligent"`; alignment=`"center"`; rows=`1`; thickness=`60`; length=`0.48` | `dock-tasks` → `dock-task-list` (`{"zone":"center","bare":true}`); `hosted-task-list` → `task-list` (`{"zone":"center","bare":true}`); `applications` → `launcher` (`{"zone":"center","bare":true}`) |

### mate-inspired — Two Panel Classic

Source: `data/profiles/mate-inspired.json`. Traditional menus above and task management below.

Default theme: `qinda-light`. Workflow: `{"overview":"disabled","workspacePolicy":"static","launcher":"classic-menu","menu":"applications-places-system","taskList":"buttons","globalMenu":false}`.

| Panel | Geometry and policy | Ordered applet instances |
| --- | --- | --- |
| menus | edge=`"top"`; layer=`"above"`; hideMode=`"never"`; alignment=`"fill"`; rows=`1`; thickness=`30`; length=`1.0` | `main-menu` → `classic-menu` (`{}`); `places` → `places-menu` (`{}`); `system` → `system-menu` (`{}`); `clipboard` → `clipboard` (`{}`); `status-notifier` → `status-notifier` (`{}`); `notifications` → `notification-center` (`{}`); `clock` → `clock` (`{}`); `applications` → `launcher` (`{}`) |
| tasks | edge=`"bottom"`; layer=`"above"`; hideMode=`"never"`; alignment=`"fill"`; rows=`1`; thickness=`34`; length=`1.0` | `show-desktop` → `show-desktop` (`{}`); `window-list` → `task-list` (`{}`); `workspaces` → `workspace-switcher` (`{}`); `tray` → `system-tray` (`{}`) |

### minimal — Minimal

Source: `data/profiles/minimal.json`. A tiny command strip for keyboard-led workflows.

Default theme: `qinda-dark`. Workflow: `{"overview":"command-palette","workspacePolicy":"dynamic","launcher":"command-palette","menu":"hidden","taskList":"hidden","globalMenu":false}`.

| Panel | Geometry and policy | Ordered applet instances |
| --- | --- | --- |
| command-strip | edge=`"top"`; layer=`"overlay"`; hideMode=`"always"`; alignment=`"center"`; rows=`1`; thickness=`26`; length=`0.3` | `workspaces` → `workspace-switcher` (`{}`); `command` → `command-palette` (`{}`); `clipboard` → `clipboard` (`{}`); `status-notifier` → `status-notifier` (`{}`); `notifications` → `notification-center` (`{}`); `clock` → `clock` (`{}`); `applications` → `launcher` (`{}`) |

### nextstep-inspired — Workspace Dock

Source: `data/profiles/nextstep-inspired.json`. A vertical application dock with workspace and utility tiles.

Default theme: `qinda-dark`. Workflow: `{"overview":"dock-grid","workspacePolicy":"static","launcher":"tile-dock","menu":"window-local","taskList":"dock-tiles","globalMenu":false}`.

| Panel | Geometry and policy | Ordered applet instances |
| --- | --- | --- |
| workspace-dock | edge=`"right"`; layer=`"above"`; hideMode=`"never"`; alignment=`"start"`; rows=`1`; thickness=`72`; length=`0.72` | `apps` → `application-tiles` (`{}`); `hosted-task-list` → `task-list` (`{}`); `workspaces` → `workspace-tiles` (`{}`); `clipboard` → `clipboard` (`{}`); `status-notifier` → `status-notifier` (`{}`); `notifications` → `notification-center` (`{}`); `clock` → `clock-tile` (`{}`); `applications` → `launcher` (`{}`) |

### qindaqt — QindaQt

Source: `data/profiles/qindaqt.json`. A calm command bar with a window-aware smart shelf.

Default theme: `qinda-dark`. Workflow: `{"overview":"compact","workspacePolicy":"static","launcher":"smart-shelf","menu":"global","taskList":"grouped","globalMenu":true}`.

| Panel | Geometry and policy | Ordered applet instances |
| --- | --- | --- |
| command-bar | edge=`"top"`; layer=`"above"`; hideMode=`"never"`; alignment=`"fill"`; rows=`1`; thickness=`30`; length=`1.0` | `brand` → `launcher` (`{"zone":"start"}`); `menu` → `global-menu` (`{"zone":"start"}`); `workspaces` → `workspace-switcher` (`{"zone":"center"}`); `status` → `system-status` (`{"zone":"end"}`); `bluetooth` → `bluetooth` (`{"zone":"end"}`); `power` → `power` (`{"zone":"end"}`); `audio` → `audio` (`{"zone":"end"}`); `clipboard` → `clipboard` (`{"zone":"end"}`); `status-notifier` → `status-notifier` (`{"zone":"end"}`); `notifications` → `notification-center` (`{"zone":"end"}`); `clock` → `clock` (`{"zone":"end"}`) |
| smart-shelf | edge=`"bottom"`; layer=`"above"`; hideMode=`"intelligent"`; alignment=`"center"`; rows=`1`; thickness=`54`; length=`0.52` | `apps` → `application-launcher` (`{"zone":"center"}`); `hosted-task-list` → `task-list` (`{"zone":"center"}`); `tasks` → `grouped-task-list` (`{"zone":"center"}`) |

### unity-inspired — Command Rail

Source: `data/profiles/unity-inspired.json`. A left launcher, global menu, and searchable command HUD.

Default theme: `qinda-dusk`. Workflow: `{"overview":"workspace-spread","workspacePolicy":"static","launcher":"left-rail","menu":"global-hud","taskList":"launcher-integrated","globalMenu":true}`.

| Panel | Geometry and policy | Ordered applet instances |
| --- | --- | --- |
| menu-bar | edge=`"top"`; layer=`"above"`; hideMode=`"never"`; alignment=`"fill"`; rows=`1`; thickness=`30`; length=`1.0` | `menu` → `global-menu` (`{}`); `hud` → `command-hud` (`{}`); `status` → `system-status` (`{}`); `clipboard` → `clipboard` (`{}`); `status-notifier` → `status-notifier` (`{}`); `notifications` → `notification-center` (`{}`); `clock` → `clock` (`{}`) |
| launcher-rail | edge=`"left"`; layer=`"above"`; hideMode=`"dodge-all"`; alignment=`"fill"`; rows=`1`; thickness=`58`; length=`1.0` | `launcher` → `application-launcher` (`{}`); `hosted-task-list` → `task-list` (`{}`); `tasks` → `grouped-task-list` (`{}`); `workspaces` → `workspace-switcher` (`{}`); `applications` → `launcher` (`{}`) |

### windows-classic — Classic Taskbar

Source: `data/profiles/windows-classic.json`. A compact menu, quick launch area, task buttons, and tray.

Default theme: `qinda-light`. Workflow: `{"overview":"disabled","workspacePolicy":"static","launcher":"start-menu","menu":"start-menu","taskList":"buttons","globalMenu":false}`.

| Panel | Geometry and policy | Ordered applet instances |
| --- | --- | --- |
| taskbar | edge=`"bottom"`; layer=`"above"`; hideMode=`"never"`; alignment=`"fill"`; rows=`1`; thickness=`36`; length=`1.0` | `start` → `start-menu` (`{}`); `quick-launch` → `quick-launch` (`{}`); `tasks` → `task-list` (`{}`); `tray` → `system-tray` (`{}`); `clipboard` → `clipboard` (`{}`); `status-notifier` → `status-notifier` (`{}`); `notifications` → `notification-center` (`{}`); `clock` → `clock` (`{}`); `applications` → `launcher` (`{}`) |

### windows-modern — Centered Taskbar

Source: `data/profiles/windows-modern.json`. A centered launcher and grouped tasks with a compact status area.

Default theme: `qinda-dusk`. Workflow: `{"overview":"task-view","workspacePolicy":"static","launcher":"centered-menu","menu":"start-menu","taskList":"grouped-icons","globalMenu":false}`.

| Panel | Geometry and policy | Ordered applet instances |
| --- | --- | --- |
| centered-taskbar | edge=`"bottom"`; layer=`"above"`; hideMode=`"never"`; alignment=`"fill"`; rows=`1`; thickness=`46`; length=`1.0` | `widgets` → `dashboard` (`{"zone":"start"}`); `task-list` → `task-list` (`{"zone":"center"}`); `center-tasks` → `centered-task-list` (`{"zone":"center"}`); `applications` → `launcher` (`{"zone":"center"}`); `status` → `system-status` (`{"zone":"end"}`); `clipboard` → `clipboard` (`{"zone":"end"}`); `status-notifier` → `status-notifier` (`{"zone":"end"}`); `notifications` → `notification-center` (`{"zone":"end"}`); `clock` → `clock` (`{"zone":"end"}`) |

### xfce-inspired — Compact Flexible

Source: `data/profiles/xfce-inspired.json`. A compact configurable panel and independent launcher dock.

Default theme: `qinda-dusk`. Workflow: `{"overview":"optional","workspacePolicy":"static","launcher":"menu","menu":"application-menu","taskList":"buttons","globalMenu":false}`.

| Panel | Geometry and policy | Ordered applet instances |
| --- | --- | --- |
| main-panel | edge=`"top"`; layer=`"above"`; hideMode=`"never"`; alignment=`"fill"`; rows=`1`; thickness=`28`; length=`1.0` | `menu` → `application-menu` (`{}`); `launchers` → `quick-launch` (`{}`); `tasks` → `task-list` (`{}`); `workspaces` → `workspace-switcher` (`{}`); `tray` → `system-tray` (`{}`); `clipboard` → `clipboard` (`{}`); `status-notifier` → `status-notifier` (`{}`); `notifications` → `notification-center` (`{}`); `clock` → `clock` (`{}`); `applications` → `launcher` (`{}`) |
| launcher-dock | edge=`"bottom"`; layer=`"above"`; hideMode=`"always"`; alignment=`"center"`; rows=`1`; thickness=`46`; length=`0.34` | `favorites` → `quick-launch` (`{}`) |

## Themes (5)

### qinda-dark — QindaPunk Nightfall

Source: `data/themes/qinda-dark.json`. 

| Field | Packaged value |
| --- | --- |
| schemaVersion | `1` |
| variant | `"dark"` |
| iconTheme | `"breeze-dark"` |
| fontFamily | `"Inter"` |
| monoFontFamily | `"JetBrains Mono"` |
| cornerRadius | `10` |
| motionDuration | `160` |
| blurEnabled | `false` |
| colors | `{"canvas":"#111E2C","surface":"#192939","surfaceRaised":"#273746","border":"#526170","text":"#F2EFE8","textMuted":"#B0B6B7","accent":"#D98A32","accentText":"#1B1309","danger":"#E86F62"}` |

### qinda-dusk — QindaPunk Dusk

Source: `data/themes/qinda-dusk.json`. 

| Field | Packaged value |
| --- | --- |
| schemaVersion | `1` |
| variant | `"dusk"` |
| iconTheme | `"breeze-dark"` |
| fontFamily | `"Inter"` |
| monoFontFamily | `"JetBrains Mono"` |
| cornerRadius | `10` |
| motionDuration | `160` |
| blurEnabled | `true` |
| colors | `{"canvas":"#192634","surface":"#243443","surfaceRaised":"#304355","border":"#5B6B7C","text":"#F4F0E8","textMuted":"#B9C0C3","accent":"#E19A3C","accentText":"#1B1309","danger":"#EC786B"}` |

### qinda-high-contrast — Qinda High Contrast

Source: `data/themes/qinda-high-contrast.json`. 

| Field | Packaged value |
| --- | --- |
| schemaVersion | `1` |
| variant | `"high-contrast"` |
| iconTheme | `"breeze-dark"` |
| fontFamily | `"Noto Sans"` |
| monoFontFamily | `"Noto Sans Mono"` |
| cornerRadius | `4` |
| motionDuration | `0` |
| blurEnabled | `false` |
| colors | `{"canvas":"#000000","surface":"#000000","surfaceRaised":"#101010","border":"#ffffff","text":"#ffffff","textMuted":"#e6e6e6","accent":"#ffdd00","accentText":"#000000","danger":"#ff5c5c"}` |

### qinda-light — QindaPunk Porcelain

Source: `data/themes/qinda-light.json`. 

| Field | Packaged value |
| --- | --- |
| schemaVersion | `1` |
| variant | `"light"` |
| iconTheme | `"breeze"` |
| fontFamily | `"Inter"` |
| monoFontFamily | `"JetBrains Mono"` |
| cornerRadius | `10` |
| motionDuration | `150` |
| blurEnabled | `false` |
| colors | `{"canvas":"#DDE7E8","surface":"#F5F7F3","surfaceRaised":"#FFFFFF","border":"#AAB7B7","text":"#17242A","textMuted":"#526066","accent":"#9B4D12","accentText":"#FFFFFF","danger":"#A63D35"}` |

### qinda-macos — Qinda macOS

Source: `data/themes/qinda-macos.json`. 

| Field | Packaged value |
| --- | --- |
| schemaVersion | `1` |
| variant | `"light"` |
| iconTheme | `"breeze"` |
| fontFamily | `"Inter"` |
| monoFontFamily | `"JetBrains Mono"` |
| cornerRadius | `12` |
| motionDuration | `180` |
| blurEnabled | `true` |
| colors | `{"canvas":"#9FB8B2","surface":"#E7EFEC","surfaceRaised":"#F7FAF9","border":"#9DAFA9","text":"#17231F","textMuted":"#60716C","accent":"#4DAF98","accentText":"#0A2921","danger":"#FF5F57"}` |
| decoration | `{"buttonPlacement":"left","tabDirection":"right-to-left","buttonStyle":"traffic-lights","hoverGlyphs":true,"closeColor":"#FF5F57","minimizeColor":"#FEBC2E","maximizeColor":"#28C840"}` |

## Applet manifests (11)

### audio — Audio

Source: `data/applets/audio.json`. Shows bounded Audio1 device and application-stream truth and offers policy-gated volume and mute controls.

| Field | Packaged value |
| --- | --- |
| schemaVersion | `1` |
| apiVersion | `"1.0"` |
| entryPoint | `{"kind":"builtin","value":"qindaqt.applets.audio"}` |
| placements | `{"zones":["panel-start","panel-center","panel-end"],"orientations":["horizontal","vertical"]}` |
| sizing | `{"mainAxis":{"minimum":40,"preferred":72,"maximum":140,"stretch":false},"crossAxis":{"minimum":24,"preferred":32,"maximum":72,"stretch":false}}` |
| capabilities | `["audio.read","audio.control"]` |
| settingsSchema | `{"type":"object","additionalProperties":false,"properties":{}}` |

### bluetooth — Bluetooth

Source: `data/applets/bluetooth.json`. Shows bounded Bluetooth1 adapter and device truth and offers policy-gated power, discovery, connect, and disconnect controls.

| Field | Packaged value |
| --- | --- |
| schemaVersion | `1` |
| apiVersion | `"1.0"` |
| entryPoint | `{"kind":"builtin","value":"qindaqt.applets.bluetooth"}` |
| placements | `{"zones":["panel-start","panel-center","panel-end"],"orientations":["horizontal","vertical"]}` |
| sizing | `{"mainAxis":{"minimum":40,"preferred":72,"maximum":140,"stretch":false},"crossAxis":{"minimum":24,"preferred":32,"maximum":72,"stretch":false}}` |
| capabilities | `["bluetooth.read","bluetooth.control"]` |
| settingsSchema | `{"type":"object","additionalProperties":false,"properties":{}}` |

### clipboard — Clipboard

Source: `data/applets/clipboard.json`. Volatile bounded clipboard history and search presentation.

| Field | Packaged value |
| --- | --- |
| schemaVersion | `1` |
| apiVersion | `"1.0"` |
| entryPoint | `{"kind":"builtin","value":"qindaqt.applets.clipboard"}` |
| placements | `{"zones":["panel-start","panel-center","panel-end","panel-fill"],"orientations":["horizontal","vertical"]}` |
| sizing | `{"mainAxis":{"minimum":32,"preferred":40,"maximum":72,"stretch":false},"crossAxis":{"minimum":24,"preferred":32,"maximum":72,"stretch":false}}` |
| capabilities | `["clipboard.read","clipboard.write"]` |
| settingsSchema | `{"type":"object","additionalProperties":false,"properties":{}}` |

### clock — Clock

Source: `data/applets/clock.json`. Displays local time and opens date and calendar presentation.

| Field | Packaged value |
| --- | --- |
| schemaVersion | `1` |
| apiVersion | `"1.0"` |
| entryPoint | `{"kind":"builtin","value":"qindaqt.applets.clock"}` |
| placements | `{"zones":["panel-start","panel-center","panel-end"],"orientations":["horizontal","vertical"]}` |
| sizing | `{"mainAxis":{"minimum":56,"preferred":96,"maximum":240,"stretch":false},"crossAxis":{"minimum":24,"preferred":32,"maximum":96,"stretch":false}}` |
| capabilities | `[]` |
| settingsSchema | `{"type":"object","additionalProperties":false,"properties":{"format":{"type":"string","enum":["locale","12-hour","24-hour"],"default":"locale"},"showSeconds":{"type":"boolean","default":false},"showDate":{"type":"boolean","default":true}}}` |

### global-menu — Global Menu

Source: `data/applets/global-menu.json`. Displays an application's exported menu through a mediated menu interface.

| Field | Packaged value |
| --- | --- |
| schemaVersion | `1` |
| apiVersion | `"1.0"` |
| entryPoint | `{"kind":"builtin","value":"qindaqt.applets.global-menu"}` |
| placements | `{"zones":["panel-start","panel-center","panel-fill"],"orientations":["horizontal"]}` |
| sizing | `{"mainAxis":{"minimum":160,"preferred":520,"stretch":true},"crossAxis":{"minimum":24,"preferred":32,"maximum":64,"stretch":false}}` |
| capabilities | `["global-menu.read","windows.activate"]` |
| settingsSchema | `{"type":"object","additionalProperties":false,"properties":{"overflow":{"type":"string","enum":["menu","scroll"],"default":"menu"}}}` |

### launcher — Application Launcher

Source: `data/applets/launcher.json`. Opens the application browser or a profile-selected launcher view.

| Field | Packaged value |
| --- | --- |
| schemaVersion | `1` |
| apiVersion | `"1.0"` |
| entryPoint | `{"kind":"builtin","value":"qindaqt.applets.launcher"}` |
| placements | `{"zones":["panel-start","panel-center","panel-end"],"orientations":["horizontal","vertical"]}` |
| sizing | `{"mainAxis":{"minimum":32,"preferred":40,"maximum":96,"stretch":false},"crossAxis":{"minimum":32,"preferred":40,"maximum":96,"stretch":false}}` |
| capabilities | `["applications.launch"]` |
| settingsSchema | `{"type":"object","additionalProperties":false,"properties":{"showRecent":{"type":"boolean","default":true},"searchOnOpen":{"type":"boolean","default":true}}}` |

### notification-center — Notification Center

Source: `data/applets/notification-center.json`. Opens the shell-owned notification center without receiving notification data or service authority.

| Field | Packaged value |
| --- | --- |
| schemaVersion | `1` |
| apiVersion | `"1.0"` |
| entryPoint | `{"kind":"builtin","value":"qindaqt.applets.notification-center"}` |
| placements | `{"zones":["panel-start","panel-center","panel-end","panel-fill"],"orientations":["horizontal","vertical"]}` |
| sizing | `{"mainAxis":{"minimum":32,"preferred":40,"maximum":72,"stretch":false},"crossAxis":{"minimum":24,"preferred":32,"maximum":72,"stretch":false}}` |
| capabilities | `[]` |
| settingsSchema | `{"type":"object","additionalProperties":false,"properties":{}}` |

### power — Power

Source: `data/applets/power.json`. Shows bounded Power1 battery truth and offers policy-gated power profile and keyboard-brightness controls.

| Field | Packaged value |
| --- | --- |
| schemaVersion | `1` |
| apiVersion | `"1.0"` |
| entryPoint | `{"kind":"builtin","value":"qindaqt.applets.power"}` |
| placements | `{"zones":["panel-start","panel-center","panel-end"],"orientations":["horizontal","vertical"]}` |
| sizing | `{"mainAxis":{"minimum":40,"preferred":56,"maximum":120,"stretch":false},"crossAxis":{"minimum":24,"preferred":32,"maximum":72,"stretch":false}}` |
| capabilities | `["power.read","power.control"]` |
| settingsSchema | `{"type":"object","additionalProperties":false,"properties":{}}` |

### status-notifier — Status Notifier

Source: `data/applets/status-notifier.json`. Presents mediated status-notifier tray items with bounded, generation-fenced activation.

| Field | Packaged value |
| --- | --- |
| schemaVersion | `1` |
| apiVersion | `"1.0"` |
| entryPoint | `{"kind":"builtin","value":"qindaqt.applets.status-notifier"}` |
| placements | `{"zones":["panel-start","panel-center","panel-end"],"orientations":["horizontal","vertical"]}` |
| sizing | `{"mainAxis":{"minimum":32,"preferred":160,"stretch":false},"crossAxis":{"minimum":24,"preferred":32,"maximum":72,"stretch":false}}` |
| capabilities | `["status-items.read","status-items.activate"]` |
| settingsSchema | `{"type":"object","additionalProperties":false,"properties":{"iconSize":{"type":"integer","minimum":16,"maximum":64,"default":22}}}` |

### system-tray — Status Tray

Source: `data/applets/status-tray.json`. Presents mediated status-notifier items and their activation actions.

| Field | Packaged value |
| --- | --- |
| schemaVersion | `1` |
| apiVersion | `"1.0"` |
| entryPoint | `{"kind":"builtin","value":"qindaqt.applets.status-tray"}` |
| placements | `{"zones":["panel-start","panel-center","panel-end"],"orientations":["horizontal","vertical"]}` |
| sizing | `{"mainAxis":{"minimum":32,"preferred":160,"stretch":false},"crossAxis":{"minimum":24,"preferred":32,"maximum":72,"stretch":false}}` |
| capabilities | `["status-items.read","status-items.activate"]` |
| settingsSchema | `{"type":"object","additionalProperties":false,"properties":{"iconSize":{"type":"integer","minimum":16,"maximum":64,"default":22},"collapsePassive":{"type":"boolean","default":true}}}` |

### task-list — Task List

Source: `data/applets/task-list.json`. Presents compositor-published windows without owning window state.

| Field | Packaged value |
| --- | --- |
| schemaVersion | `1` |
| apiVersion | `"1.0"` |
| entryPoint | `{"kind":"builtin","value":"qindaqt.applets.task-list"}` |
| placements | `{"zones":["panel-start","panel-center","panel-fill"],"orientations":["horizontal","vertical"]}` |
| sizing | `{"mainAxis":{"minimum":120,"preferred":480,"stretch":true},"crossAxis":{"minimum":28,"preferred":40,"maximum":144,"stretch":false}}` |
| capabilities | `["windows.read","windows.activate","windows.manage"]` |
| settingsSchema | `{"type":"object","additionalProperties":false,"properties":{"grouping":{"type":"string","enum":["never","when-crowded","always"],"default":"when-crowded"},"rows":{"type":"integer","minimum":1,"maximum":8,"default":1}}}` |

## Default applet capability policy

Source: `data/applet-policy/default.json`. The policy is interpreted by the host/runtime boundary; a requested capability is not automatically granted.

```json
{
  "schemaVersion": 1,
  "defaults": {
    "auditedBuiltin": "grant",
    "thirdParty": "deny"
  },
  "rules": [
    {
      "trust": "third-party",
      "packageId": "*",
      "capability": "applications.launch",
      "decision": "grant",
      "reason": "May request application launch through the future mediated launcher interface."
    },
    {
      "trust": "third-party",
      "packageId": "*",
      "capability": "settings.read",
      "decision": "grant",
      "reason": "May read only its namespaced settings through the future settings mediator."
    },
    {
      "trust": "third-party",
      "packageId": "*",
      "capability": "windows.manage",
      "decision": "deny",
      "reason": "General third-party applets may not mutate compositor-owned window state."
    },
    {
      "trust": "audited-builtin",
      "packageId": "global-menu",
      "capability": "global-menu.read",
      "decision": "grant",
      "reason": "The audited shell composition may own the standard registrar and read the authenticated active provider's dbusmenu endpoint."
    },
    {
      "trust": "audited-builtin",
      "packageId": "global-menu",
      "capability": "windows.activate",
      "decision": "deny",
      "reason": "Menu Event delivery does not grant general compositor window activation authority."
    },
    {
      "trust": "audited-builtin",
      "packageId": "clipboard",
      "capability": "clipboard.read",
      "decision": "grant",
      "reason": "The audited Clipboard applet may observe the bounded metadata-only history projection through its injected client seam."
    },
    {
      "trust": "audited-builtin",
      "packageId": "clipboard",
      "capability": "clipboard.write",
      "decision": "grant",
      "reason": "The audited Clipboard applet may dispatch generation-fenced select, pin, delete, and clear intents; execution stays with the clipboard authority."
    },
    {
      "trust": "third-party",
      "packageId": "task-list",
      "capability": "windows.read",
      "decision": "deny",
      "reason": "Window inventory observation stays with the audited Task List applet; third-party packages have no compositor window read authority."
    },
    {
      "trust": "third-party",
      "packageId": "task-list",
      "capability": "windows.activate",
      "decision": "deny",
      "reason": "Window activation stays with the audited Task List applet's generation-fenced intents; third-party packages may not activate windows."
    },
    {
      "trust": "audited-builtin",
      "packageId": "status-notifier",
      "capability": "status-items.read",
      "decision": "grant",
      "reason": "The audited Status Notifier applet may observe the bounded, validated status-item registry through its injected source seam."
    },
    {
      "trust": "audited-builtin",
      "packageId": "status-notifier",
      "capability": "status-items.activate",
      "decision": "grant",
      "reason": "The audited Status Notifier applet may dispatch generation-fenced activate, secondary-activate, and context-menu intents; execution stays with the item owners."
    }
  ]
}
```

## Session entry

`data/session/qindaqt.desktop.in` supplies the display-manager session template. Session boot, residency, and teardown contracts are in [Compositor and session integration](../../architecture/compositor-session.md).
