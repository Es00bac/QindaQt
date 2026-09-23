# Settings Date & time route

The `datetime` route is QindaQt Settings' clock and region page. It was
appended after Streaming without moving earlier route indices or digit
shortcuts; later routes followed it in the stable registry order.

Its platform boundary is [ADR-0211](../adr/0211-the-clock-and-region-page-acts-on-the-platforms-own-services.md); [ADR-0249](../adr/0249-confirm-week-start-writes-against-settings1.md) defines the week-start outcome contract.

## What it changes, and what it only shows

| Control | Acts on | Editable |
| --- | --- | --- |
| Time zone | `org.freedesktop.timedate1` `SetTimezone` | yes, from the list the platform itself provides |
| Set automatically | `org.freedesktop.timedate1` `SetNTP` | yes, when `CanNTP` |
| Clock synchronization | `NTP` / `NTPSynchronized` | read-only status |
| First day of the week | Settings1 `services.calendarWeekStart` | only with a fresh, admissible Settings1 baseline |
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

## First day of the week confirms before it moves

The Calendar reads `services.calendarWeekStart`, and this route edits precisely
that one Settings1 key. The picker displays the last confirmed value. It is
disabled until the client has a fresh snapshot from its current exact owner and
no request or write blocks admission. A retained value during owner replacement
or a degraded/refreshing client remains visible for reference but is not
editable. The calendar preference has its own availability card even when the
system clock service is unavailable.

A submitted change shows a pending card without moving the picker. Immediate
admission refusal, later validation/persistence refusal, revision conflict,
owner loss, including owner-to-owner replacement while the client remains
Authenticating, timeout and malformed/lost replies each have a visible result.
An Applied reply alone is not completion: the picker changes only on a new
same-owner/epoch snapshot at or above the committed revision. An older snapshot
keeps the request pending until a bounded readback deadline. A newer snapshot
with a different value reports a conflict rather than success. An uncertain
write is never automatically replayed. **Retry reading calendar preference**
requests a fresh snapshot only, leaving the previous diagnostic visible until
another explicit edit; the displayed choice always follows the latest
confirmed value, including external edits.

## Unavailability is stated

- A clock service that cannot be reached makes the whole page say so, rather
  than showing empty fields that look like real values.
- A machine with no time-synchronization service shows automatic time as
  **unavailable**, not merely off.
- A platform that will not list its zones leaves the current zone visible and
  the picker closed — an empty picker looks broken.
- A Settings1 service that is not ready leaves the first-day-of-week control
  unavailable rather than presenting the schema default as the user's choice.
  Its Retry action re-reads only and never repeats an uncertain write.

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

`qindaqt.settings-week-start-preference` exercises the production adapter
with an injected fake SettingsTransport: initial absence, exact admission,
pending and revision-floor readback, refusal, conflict, lost reply, owner
replacement, external updates, and no replay. `qindaqt.settings-client` checks
that the public admission preview follows snapshot-request and owner changes.

Both bus addresses are pointed at a nonexistent socket for those rows, so "no
bus is touched" is enforced rather than intended. **No row calls timedate1** —
one that did would change the developer's own clock — so an actual timezone
change through polkit has not been observed and needs an installed build.

`qindaqt.settings-route-registry` and
`qindaqt.settings-navigation-controller` pin the route count (12 → 13), the
route's index (12), and that every earlier index is unmoved.
