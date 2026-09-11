# ADR-0136: Night light through KWin and knighttimed

- Status: Accepted
- Date: 2026-09-11

## Context

Settings → Display has no night light section, although the pinned KWin 6.6.5
ships the `nightlight` plugin (live at
`org.kde.KWin.NightLight`, `/org/kde/KWin/NightLight`), and the installed
`kde-plasma/knighttime` provides the dark-light schedule daemon `knighttimed`
(name `org.kde.NightTime`, interface `org.kde.NightTime.Manager`). QindaQt must
gain the user-visible outcome — on/off, schedule (sunset to sunrise by automatic
location or manual coordinates, custom times, or always on), live-previewed
night and day temperatures, a transition length, and a truthful status line —
without competing with KWin or the schedule daemon for authority. KWin remains
the only live-output authority
([display service](../architecture/display-service.md)); the ICC path through
output management ([ADR-0083](0083-apply-saved-color-profiles-through-public-output-management.md))
is untouched.

## Decision

**Authorities.** KWin's nightlight plugin owns the live output; `knighttimed`
owns schedule computation. QindaQt never computes solar times, never runs a
second schedule authority, and never writes `knighttimestaterc` (the daemon's
own geolocation cache, per upstream `kdarklightstate.kcfg`). QindaQt reads live
state only from the KWin D-Bus properties and caches nothing as truth.

**Persistence.** QindaQt writes exactly these keys, and nothing else:

- `kwinrc [NightColor]` — `Active` (Bool), `Mode`
  (`Constant`/`DarkLight` choice-name strings), `DayTemperature` (Int),
  `NightTemperature` (Int). Source of truth:
  `/usr/share/config.kcfg/nightlightsettings.kcfg` in the pinned KWin, whose
  `<choices name="KWin::NightLightMode">` order makes `Constant`=0,
  `DarkLight`=1. The installed D-Bus introspection XML documents mode as
  "0 automatic … 3 constant"; that documentation is outdated and the kcfg wins.
- `knighttimerc` — group `General` key `Source` (`Location`/`Times` strings),
  group `Location` keys `Automatic` (Bool), `Latitude`/`Longitude` (Doubles,
  decimal degrees), group `Times` keys `SunriseStart`/`SunsetStart`
  (`HH:mm:ss`), `TransitionDuration` (UInt seconds). Source of truth: upstream
  knighttime v6.6.6 `src/daemon/kdarklightsettings.kcfg` and
  `kdarklightmanager.cpp`, which opens
  `KSharedConfig::openConfig("knighttimerc")` and watches it through
  `KConfigWatcher`, so schedule writes apply live while the daemon runs.
  The upstream migration multiplies legacy minutes by 60, confirming the
  stored unit is seconds. Enum entries persist as choice-name strings, as the
  upstream migration's string comparisons prove.

The night light service writes both files through one injected config port that
preserves unrelated groups and keys, skips writes whose values did not change,
and fails closed when a path cannot be written. KDE's `KConfig` watcher
machinery (inotify based) notices the file change, so no KF6 dependency is
added to the QindaQt service module; the value shapes written (bool
`true`/`false`, enum name strings, `HH:mm:ss`, C-locale doubles) are exactly
the ones KConfigXT reads.

**Mode mapping.** "Always on" is `Mode=Constant` with `Active=true`. Every
scheduled choice is `Mode=DarkLight` with `Active=true` plus the matching
knighttimerc schedule keys: sunset to sunrise by automatic location is
`Source=Location` with `Automatic=true` (only when the user picks it), manual
coordinates are `Source=Location` with `Automatic=false` plus the coordinates,
and custom times are `Source=Times` plus `SunriseStart`/`SunsetStart`/
`TransitionDuration`.

**Bounds.** The kcfg declares no temperature range, so the model bounds night
and day temperatures to 1000–6500 K in 100 K steps (6500 K is KWin's neutral
point). Latitude is bounded to ±90, longitude to ±180, and the transition
length to 60–7200 seconds (default 1800).

**Preview.** The temperature slider previews through the KWin `preview(u)`
call, which holds one temperature for 15 seconds and is replaced by a later
call. The section debounces drags so exactly one preview call is in flight per
settled value, and calls `stopPreview` when the slider is released without
applying or when the page closes. Applying a draft writes the config; the
preview never substitutes for a write.

**Inhibition.** `inhibit()` is connection-scoped by interface contract — the
installed D-Bus XML states the lock is "released automatically when the
service, which requested it, is unregistered" — and the private headless proof
confirmed that the lock disappears when the caller disconnects. A Settings page
process cannot hold a meaningful pause, so Settings offers no pause control. A
resident shell quick-toggle is the recorded follow-up; it can hold an
inhibition for its own process lifetime.

**Location privacy.** Automatic location is enabled only when the user chooses
"sunset to sunrise (automatic location)"; that choice lets `knighttimed` use Qt
Positioning on its own behalf. QindaQt never reads positioning data, never
sees the resolved coordinates, and never writes the daemon's state cache. The
manual-coordinate fields are stored only in `knighttimerc` as the user typed
them.

**Fail closed.** When `org.kde.KWin.NightLight` is absent the whole section
reports unavailable and disables its controls. When `org.kde.NightTime` is
absent the schedule controls are disabled with a truthful reason while the
on/off and temperature controls stay usable, because `kwinrc` applies
regardless. A status frame with a hostile or out-of-range value (unknown mode
integer, temperature outside the documented neutral/bounds window, non-numeric
type) is rejected whole: the last complete status stays published and the port
reports degraded truth instead of partial values.

## Alternatives considered

- Computing sunrise/sunset inside QindaQt: rejected — it duplicates
  `libKNightTime`, drifts from KWin's actual schedule, and adds a second
  authority for the same output.
- Offering pause from Settings via `inhibit()`: rejected — the lock is
  connection-scoped and dies with the caller; the UI would lie about a held
  pause.
- Writing `knighttimestaterc`: rejected — it is the daemon's private
  geolocation cache, not a settings surface.
- Extending Display1 to carry night light: rejected — Display1 is the topology
  and output-management authority; night light has its own public KWin
  interface and config schema.

## Consequences

Settings → Display gains the night light section backed by
`src/services/night_light`, with the contract in
[night light](../architecture/night-light.md). Packaging must add
`kde-plasma/knighttime` to RDEPEND so scheduled night light works on a fresh
install (requested from the Program Manager). The shell quick-toggle that can
hold a real inhibition remains a follow-up outside this wave. Downstream
consumers must keep reading live state from D-Bus and must not parse KWin or
daemon config as state truth.
