# ADR-0211: The clock and region page acts on the platform's own services, and shows only what it can change

- **Status:** Proposed
- **Date:** 2026-09-17
- **Owners:** Settings
- **Supersedes:** None
- **Superseded by:** None

## Context

PLAN.md §3 O13 lists "Date, time & region" among the routes a desktop user
expects: timezone and NTP through `org.freedesktop.timedate1`, clock format,
first day of week, and locale through `org.freedesktop.locale1`. Both services
are present and answering on this machine (`Timezone "America/Denver"`,
`NTP true`, `CanNTP true`, `Locale ["LANG=en_US.UTF-8"]`).

Two of the four items turned out not to belong on this page, and one of them
cannot be made editable honestly.

Writing a system-wide clock setting is privileged. systemd's own answer is the
`interactive` flag: the call goes out, polkit asks the user through their own
authentication agent, and a refusal comes back as a D-Bus error. A settings
page that moved its controls before that answer arrived would tell the user
their timezone changed when it had not.

## Decision

The route is registered as `datetime`, last, so **no existing route's index
moves** — shortcuts and traversal depend on that order.

- **Time zone** and **automatic time** are editable through timedate1, always
  with `interactive = true`, so the user sees their own authentication prompt
  and a refusal is reported. Only a zone the platform itself listed is ever
  requested.
- **Nothing moves optimistically.** Every published value is what the platform
  last reported; a request sets a busy flag and nothing else, and the page
  re-reads after a write rather than trusting it. Controls stay *enabled*
  while busy — disabling them would make a slow authentication prompt look
  like a hang.
- **Unavailability is stated, not implied.** A clock service that cannot be
  reached makes the page say so instead of showing empty fields. A machine
  with no time-synchronization service shows automatic time as *unavailable*
  rather than merely off. A platform that will not list its zones leaves the
  current zone visible and the picker closed, because an empty picker looks
  broken.
- **First day of the week** is the Settings1 key `services.calendarWeekStart`,
  which the Calendar's month grid already reads and which had no editor
  anywhere. Its client is **purpose-scoped to that one key**, the way
  `CalendarPreferences` scopes its own client: a `SettingsClient` takes the
  exact key list it wants, and keeping this one at a single key is what stops
  the page from subscribing to settings it has no business seeing. No schema
  key is added, so there is no migration and no resident-service restart.
- **The system locale is shown read-only.** `locale1` offers no way to
  enumerate the locales a machine has actually generated, and a free-text
  field that can leave the session with an unusable locale is worse than a
  value the user can read here and change with `localectl`. The page says so
  in those words.
- **Clock format is not on this page.** The panel clock's 12/24-hour choice,
  seconds and date are that applet's own settings (`data/applets/clock.json`),
  edited where the clock lives. Duplicating them here would create two places
  that disagree.

The seam is `SystemTimeService`, and the production `SystemdTimeService`
touches exactly four things: `Timezone`, `NTP`/`CanNTP`/`NTPSynchronized`,
`ListTimezones`, and locale1's `Locale`. `SetTime`, `SetLocalRTC`,
`SetVConsoleKeyboard` and `SetX11Keyboard` are deliberately outside it.

## Consequences

- Every control on the page acts on something real. That is not a
  platitude here: the five `windowManagement.*` keys the "Windows &
  workspaces" route would have edited have **no consumer anywhere in the
  tree**, which is why that route is not in this change.
- No new dependency and no new schema key.
- The route count moves from 12 to 13; `tst_settings_route_registry` and
  `tst_settings_navigation_controller` pin it and are updated with it.
- Focused row `qindaqt.settings-datetime-model` proves the model over the
  seam's own fakes, with both bus addresses pointed at a nonexistent socket so
  "no bus is touched" is enforced rather than intended. **No row calls
  timedate1**: one that did would change the developer's own clock.
- Because no row calls the real service, an actual timezone change through
  polkit has not been observed. That needs an installed build and a user
  answering a prompt.

## Revisit when

`locale1` gains a way to enumerate installed locales (or QindaQt gains a
vetted catalogue), which is what the locale control is waiting for; or a
second QindaQt surface needs the first-day-of-week value, at which point its
purpose-scoped client should become a shared one.
