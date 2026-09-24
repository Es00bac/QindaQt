# Settings completeness inventory

This is the source-backed inventory of the 21 routes registered by
`SettingsRouteRegistry::registerBuiltInRoutes()` and
`registerAppendedRoutes()` on the integrated qinda source branch (2026-09-23). Registration
and the [active Loader witness](../adr/0250-require-active-loader-witness-for-every-settings-route.md)
prove that each route can construct or show its declared unavailable state.
They do not prove physical hardware behavior or that every stored schema key
has a consumer. The [Settings1 key catalog](../handbook/catalog/settings.md)
classifies all 85 storage keys separately; many routes use native service
authority outside Settings1.

| Order | Route ID | Current owned surface | Boundary or remaining gap |
| --- | --- | --- | --- |
| 1 | `notifications` | Do Not Disturb and [Quiet Hours schedule](../shell/notification-presentation.md) | Shell interruption policy consumes these preferences. No per-app allow/quiet editor or verified `services.notifications` master-key consumer. |
| 2 | `appearance` | [Theme, wallpaper, fonts (including monospace), and window/container decoration](../apps/appearance-settings.md) | Some retained appearance schema keys have no verified runtime consumer; see catalog. |
| 3 | `display` | [Output arrangement, mirror, orientation, resolution/scale, refresh and night light](../apps/display-settings.md) | Display1/KWin own topology; legacy `displays.*` topology keys are not another editor. HDR behavior is not established by the old `displays.hdrPolicy` default. |
| 4 | `network` | [Wi-Fi, saved/visible networks, devices and admitted Wi-Fi/mobile radio switches](../apps/network-settings.md) | Network1 owns mutation and hardware refusal; credentials stay with the separate agent. No live radio test is claimed here. |
| 5 | `customize` | [Panel layout profiles, applets and direct panel editing](../apps/customize-settings.md) | Customize edits panel reveal/hide timing through the shell visibility policy; the customization chord still lacks a verified editor. |
| 6 | `audio` | [Devices, per-app stream routing, virtual devices, mixing console and manual stereo peers](../apps/audio-settings.md) | Audio1 owns device/graph truth. Manual peers need schema-12 service on both hosts; local active links do not prove remote audibility. |
| 7 | `bluetooth` | [Adapters, discovery, pairing replies and devices](../apps/bluetooth-settings.md) | BlueZ/service own pairing and hardware truth; stored `services.bluetooth` has no verified master-key consumer. |
| 8 | `power` | [Supplies, profiles, brightness, lid/button policy, screen lock and idle display power](../apps/power-settings.md) | Power1, Display1, login1 and PowerDevil own effects; hardware and authorization vary by host. Screen saver selection has its own route. |
| 9 | `clipboard` | [History consent, status and confirmed clear](../apps/clipboard-settings.md) | Clipboard1 owns private content; Settings shows bounded metadata, not copied items. |
| 10 | `color` | [ICC catalog, import and per-output profile assignments](../apps/color-settings.md) | Assignment/store readback is documented; construction tests are not physical color calibration. |
| 11 | `accessibility` | [Contrast, motion, transparency and text scale](../apps/accessibility-settings.md) | The screen-reader schema key is reserved and intentionally hidden until a provider/session contract exists. |
| 12 | `input` | [Pointer/touchpad, keyboard, shortcuts, touch and tablet mapping](../apps/input-settings.md) | KWin owns native pointer/keyboard changes; old generic input keys are superseded. `input.touch.mode` is reserved. |
| 13 | `streaming` | [OBS setup, connection preferences, login start and console bus map](../apps/streaming-settings.md) | OBS and its bridge own actual connection; settings outcomes and owner truth do not prove an OBS session is reachable. |
| 14 | `datetime` | [Timezone, NTP, system status and first day of week](../apps/datetime-settings.md) | `timedate1` owns writes; system locale is read-only and clock format belongs to the panel clock. |
| 15 | `windows` | [Focus, docking modifier, snap distance and close-container policy](../apps/windows-settings.md) | Settings separates confirmed saved values from exact-owner KWin session apply status for four keys; `sessionRestore` remains reserved and hidden. |
| 16 | `default-apps` | [Supported MIME default categories](../apps/default-applications.md) | XDG preference files and installed handlers own effective choices; terminal has no standard MIME-default category. |
| 17 | `about-computer` | [Read-only host/hardware/software/usage facts and copy report](../apps/about-this-computer.md) | Failed sources show unavailable per field; this route has no mutation authority. |
| 18 | `startup` | [XDG autostart entries and add-a-command](../apps/startup-settings.md) | Session consumes eligible entries once per login; D-Bus activation and early phases remain documented limits. |
| 19 | `screensaver` | [Saver selection, delay, preview and lock-screen mirror](../apps/screensaver-settings.md) | Saver availability depends on installed packages; independent idle-lock preference remains with Power. |
| 20 | `login-screen` | [SDDM theme, autologin/session, numeric lock and cursor theme](../apps/login-screen-settings.md) | Privileged helper/polkit and SDDM configuration own writes; no live greeter reboot test is implied. |
| 21 | `voice` | [Desktop voice opt-in, provider preference and panel transcript choice](../apps/voice-settings.md) | Confirmed preference gates QindaQt activation; an independently running provider is not terminated. |

The first ten routes keep Ctrl+1 through Ctrl+0 respectively. Routes 11–21
are reached from the wide sidebar or compact tabs and retain their registry
order. A route's unavailable diagnostic is a supported construction result,
not proof that its backing service or device is present on this computer.

## Cross-route gaps and ownership

- The [Settings1 catalog](../handbook/catalog/settings.md) identifies keys with
  active consumers but no editor (such as `shell.customization.chord`),
  legacy native-authority keys, reserved keys
  and storage entries with no verified consumer. Do not turn a default value
  into a user-facing switch until its consumer and outcome contract exist.
- App-owned preferences such as Calendar view, Text Editor restore, launcher
  history and shell shortcut-help dismissal belong at those owning surfaces,
  not in a duplicate generic Settings page.
- System locale editing, arbitrary Windows session restoration, a screen-reader
  enable switch, global Bluetooth/notification service master switches,
  physical network/audio qualification, and synchronized surround audio are
  not established by the current route inventory. Their absence or limits are
  recorded in the owning route docs and schema catalog.

This inventory replaces the historical 12/13-route snapshot. It describes
checked-in boundaries at the named commit; a later integration should compare
it with the route registry and schema before changing counts or completion
claims.
