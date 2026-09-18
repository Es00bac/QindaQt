# Settings Date & time route

The `datetime` route is QindaQt Settings' clock and region page. It is the
thirteenth built-in route and is registered **last**, so no existing route's
index moves — shortcut and traversal order depend on that.

Its decision is [ADR-0200](../adr/0200-the-clock-and-region-page-acts-on-the-platforms-own-services.md).

## What it changes, and what it only shows

| Control | Acts on | Editable |
| --- | --- | --- |
| Time zone | `org.freedesktop.timedate1` `SetTimezone` | yes, from the list the platform itself provides |
| Set automatically | `org.freedesktop.timedate1` `SetNTP` | yes, when `CanNTP` |
| Clock synchronization | `NTP` / `NTPSynchronized` | read-only status |
| First day of the week | Settings1 `services.calendarWeekStart` | yes |
| System locale | `org.freedesktop.locale1` `Locale` | **read-only** |

Two things PLAN.md grouped under "Date, time & region" are deliberately not
here.

**Clock format** — 12 or 24 hour, seconds, the date — belongs to the panel
clock applet (`data/applets/clock.json`) and is edited where that clock lives,
in Customize. Putting it here too would create two places that disagree. The
page says so.

**The system locale is shown, not edited.** `locale1` offers no way to
enumerate the locales a machine has actually generated, so the only honest
editor would be a free-text field — and a typo there leaves the session with
an unusable locale. The page shows the value and names `localectl` as the way
to change it.

## Nothing moves until the platform says so

Changing a system-wide clock setting is privileged. Every write goes out with
systemd's `interactive` flag set, so the user's own authentication agent asks
and a refusal comes back as a D-Bus error the page reports.

- Published values are always what the platform last reported, never what the
  user just asked for. A refusal therefore leaves every control where it was,
  with the reason on screen.
- After a successful write the page **re-reads** rather than trusting the
  write.
- Controls stay enabled while a request is outstanding. Disabling them would
  make a slow authentication prompt look like a hang.
- `PropertiesChanged` on both services is subscribed, so a change made with
  `timedatectl` or another settings application shows up here too.

## Unavailability is stated

- A clock service that cannot be reached makes the whole page say so, rather
  than showing empty fields that look like real values.
- A machine with no time-synchronization service shows automatic time as
  **unavailable**, not merely off.
- A platform that will not list its zones leaves the current zone visible and
  the picker closed — an empty picker looks broken.
- A Settings1 service that is not ready leaves the first-day-of-week control
  unavailable rather than presenting the schema default as the user's choice.

## Boundaries

`SystemTimeService` is the injected seam, and the production
`SystemdTimeService` touches exactly four things: `Timezone`, the NTP trio,
`ListTimezones`, and locale1's `Locale`. `SetTime`, `SetLocalRTC`,
`SetVConsoleKeyboard` and `SetX11Keyboard` are deliberately outside it — this
page does not set the wall clock, does not touch the RTC mode, and does not
change a keymap.

The first-day-of-week client is **purpose-scoped to its single key**, the same
way `CalendarPreferences` scopes its client to the three keys the Calendar
reads. A `SettingsClient` is constructed with the exact key list it wants, and
keeping that list at one key is what stops this page from subscribing to — or
being disturbed by — settings it has no business seeing.

The page's own composition uses two buses on purpose: timedate1 and locale1 are
system-wide services on the system bus, Settings1 is the user's own session
service, and nothing crosses between them.

## Focused rows

`qindaqt.settings-datetime-model` proves the model over the seam's own fakes:
the startup read, the published snapshot, an unavailable service, requesting
only a real change and only a zone the platform offered, a refusal leaving
every value alone, automatic time without support, the read-only picker, and
the first-day-of-week preference including a refused write.

Both bus addresses are pointed at a nonexistent socket for that row, so "no
bus is touched" is enforced rather than intended. **No row calls timedate1** —
one that did would change the developer's own clock — so an actual timezone
change through polkit has not been observed and needs an installed build.

`qindaqt.settings-route-registry` and
`qindaqt.settings-navigation-controller` pin the route count (12 → 13), the
route's index (12), and that every earlier index is unmoved.
