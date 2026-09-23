# Settings completeness audit

Every route `qindaqt-settings` registers (`settings_route_registry.cpp`,
`registerBuiltInRoutes()`), what each one covers, and the routes a desktop
user expects that do not exist yet. The survey below the fold (everything
except the `startup` row) reflects `main` @ `4815ef9a`, 12 routes, written
first per PLAN.md's "audit table first" instruction; the `startup` row was
added once that route shipped later in this same lane. Status column:
**exists** (the row/control is implemented and wired to a real authority),
**gap** (the route exists but is missing a control a user would expect
there), **deferred** (named in PLAN.md but explicitly out of this wave — not
a defect of the current route).

## Registered routes

| Route (id) | Sections | Status | Notes |
| --- | --- | --- | --- |
| `notifications` | Do Not Disturb switch only | **gap** | No schedule, no per-app allow/quiet list, no sound-on-notification toggle. `NotificationsPage.qml` is 110 lines, a single `Switch`. PLAN.md names this route explicitly ("extend the existing notifications page if it is partial") — it is partial. Not fixed in this slice (not in my claimed scope; flagged for a follow-up). |
| `appearance` | Theme, Wallpaper, Fonts, Application windows | exists | Covers theme/color scheme/fonts/wallpaper/scaling per PLAN's "Windows & workspaces" ask only for window *decoration* choice, not focus policy — see the `windows` gap below. |
| `display` | Displays (arrangement/mirror), Resolution & scale, Orientation, Night light | exists | Checkpoint A / ADR-0190: mirror, rotation, refresh rate, primary, `Meta+P` mode switcher. Already integrated; not re-audited row-by-row here. |
| `network` | Wi-Fi networks, Radios, Network devices, Saved networks | exists | Wireless/adapter surface. This is a *different* route from O7's file-manager "Connect to server" network-locations feature (SMB/SFTP saved locations for the file manager, tracked separately under O7, not a Settings route). |
| `customize` | Panels, applets, placement, layout profiles | exists | O9's live in-place editor is additive to this route, not a replacement. |
| `audio` | Output/input devices, Application streams, Virtual devices, Mixing console | exists | O5 (this wave) fixed the pending/coalescing bug; console/mixing surfaces are O6's redesign target, functionally complete today. |
| `bluetooth` | Adapters, Pairing request, Devices | exists | |
| `power` | Power supplies, Power mode (PPD profiles) + holds, Session, Screen lock, Power button and lid, Internal/external/keyboard brightness, Display power (idle) | exists | O8 (this wave) verified lid/suspend/lock and added battery notifications; AC-vs-battery *automatic* profile switching is a documented open gap (`docs/wiki/reference/laptop-readiness-qinda-top.md`), not this route's row-level completeness. |
| `clipboard` | Private history preference, state, clearing | exists | |
| `color` | ICC profile catalog, Displays, Disconnected displays, Import | exists | |
| `accessibility` | Contrast, motion, transparency, text scale | exists | Single page (`AccessibilityPage.qml`); not sectioned like the others, but covers its stated scope. |
| `input` | Mouse & touchpad, Keyboard, Shortcuts | exists | O8 (this wave) fixed a real bug: tap-to-click/tap-and-drag rows were permanently hidden by a KWin property-name mismatch, independent of hardware. |
| `startup` | Add a command, Startup entries | exists | New this slice; see `docs/wiki/apps/startup-settings.md`. The session now consumes eligible entries once per login, with TryExec and desktop filters shared with the Settings ineligibility display; see ADR-0247. D-Bus activation and early startup phases remain diagnosed limitations. |
| `screensaver` | Screen saver (discovered saver choice, idle delay, preview), Locking (walk-away idle lock) | exists | ADR-0226: split out of the Power route, which no longer carries a Screensaver section. Saver list is discovered from installed packages, so "None"/"Blank screen" are the only always-present choices. |

## Routes named in PLAN.md §3 that do not exist yet

| Route | PLAN.md's ask | Status | This slice |
| --- | --- | --- | --- |
| Streaming | OBS connection, autostart, defaults, bus map | **deferred** | O10's route; not started, correctly out of scope here. |
| Windows & workspaces | Focus policy, raise-on-hover, title-bar actions, docking modifier, customization chord, workspace count/names, switching animation | **deferred** | No route exists. Distinct from `appearance`'s window-*decoration* choice above. Not in this slice's claimed paths; a real gap for a future O13 continuation. |
| Default applications | Browser, mail, terminal, file manager, text editor, image/video/music players via `xdg-mime`/`xdg-settings` | **deferred** | Not started in this pass. `xdg-mime`/`xdg-settings` need an installed-application catalog (scanning `MimeType=` across every `.desktop` file on the system) this slice did not have time to build alongside Startup applications; a real follow-up, not a defect. |
| Date, time & region | Timezone/NTP via `org.freedesktop.timedate1`, clock format, first day of week, locale via `org.freedesktop.locale1` | **deferred** | No route exists. Not in this slice's claimed paths. |
| Startup applications | XDG autostart entries, enable/disable, add-a-command | **filled in this slice** | See `docs/wiki/apps/startup-settings.md` and [ADR-0214](../adr/0214-startup-applications-route.md). |
| Notifications (extend) | DND schedule, per-app allow/quiet, sound on notification | **deferred** | See the `notifications` row above; extending the existing partial page is a bounded follow-up, not started here. |
| About this computer | Hostname, hardware, kernel, QindaQt version + checkpoint, disk, memory, battery health, "copy report" | **deferred** | Not started in this pass; see "What this slice actually shipped" below. |

## What this slice actually shipped

This slice's original claim named three new routes (Startup applications,
Default applications, About this computer). Only **Startup applications**
shipped as a complete, wired, tested route this pass — see
`docs/wiki/apps/startup-settings.md`. Default applications and About this
computer are real gaps, not fabricated ones, and are recorded as
**deferred** above rather than claimed done: each is its own bounded slice
(Default applications needs an installed-application MIME catalog; About
needs a `hostnamed`/`sysinfo`/UPower-backed read-only info surface plus a
"copy report" clipboard action), and building all three to the same
completeness this pass reached for one would have meant shipping partial,
untested wiring for two of them — worse than shipping one route that is
actually finished.

## Non-claims

This audit does not re-derive Checkpoint A's already-integrated display
work, does not re-litigate O5/O6's audio scope split, and does not attempt a
control-by-control accessibility or keyboard-navigation audit of the
existing 12 (now 13) routes — that would be its own lane. It records
route-level and named-section-level completeness only.
