# QindaQt Calendar

`qindaqt-calendar` is QindaQt's first-party local calendar. Milestone 1
landed month/week/day browsing over local RFC 5545 calendars, event creation,
deletion, ICS import/export, Settings1 preferences, and reminder delivery.
The milestone finishing slice adds real event editing — an in-place,
uid-stable update through the model layer — and an event details pane with
Edit and Delete actions. Per
[ADR-0116](../adr/0116-build-bundled-applications-on-stock-qt6.md) the
presentation is stock Qt Quick Controls themed by the Qt platform theme
([ADR-0115](../adr/0115-share-appearance-through-qt-platform-theme.md));
the app imports no `QindaQt.Tokens`/`QindaQt.Controls` modules and carries no
per-app palette or theme option.

## Architecture and boundaries

The module layout under `src/apps/calendar/` keeps policy, persistence, and
presentation separate (see
[Module boundaries](../architecture/module-boundaries.md)):

- `model/` owns all domain logic. `EventStore` is the persistence authority:
  one `.ics` file per calendar under `$XDG_DATA_HOME/qindaqt/calendar`, every
  mutation persisted immediately through an atomic `QSaveFile` replace with
  rollback on failure. `CalendarCollection` owns calendar metadata and the
  default calendar in `calendars.json`. `OccurrenceExpander` turns
  KCalendarCore recurrence into concrete occurrences for the visible range;
  `ReminderScheduler`/`ReminderDelivery` post due reminders to the
  freedesktop notification server and fall back to an in-window banner when
  the bus is unavailable.
- `CalendarController` is the only QML-facing bridge. QML never touches
  KCalendarCore, never triggers persistence or expansion directly, and stays
  presentation-only. KF6CalendarCore is confined to `model/` and the
  controller; it is not a public dependency of the app.
- `settings/calendar_preferences.*` reads the three `services.calendar*`
  Settings1 keys (default view, week start, default calendar) and degrades to
  schema defaults without a session bus.
- `app_shell/calendar_action_catalog.*` declares the deterministic action
  catalog (File/Edit/View/Go/Calendar menus); `main.cpp` binds the injected
  `ApplicationCoordinator` to the stock `ApplicationWindow` and composes the
  fail-closed first-party global-menu export
  (`composeCalendarMenuExport`). The in-window `MenuBar` remains
  authoritative when no shell hosts the menu.

## Features

- Month, week, and day views with `Ctrl+1/2/3`, Today/previous/next period
  navigation, and a calendars sidebar with per-calendar visibility toggles
  and new-calendar creation.
- Event creation and **editing** through one dialog: summary, calendar
  (create only), start/end, all-day, location, description, recurrence
  preset (none/daily/weekly/monthly/yearly), and reminder offset
  (none/5/10/15/30/60 minutes). Editing loads every stored field and saves
  as an in-place update.
- Selecting an occurrence in any view opens a details pane showing summary,
  calendar, when, recurrence, reminders, location, and description, with
  Edit and Delete actions.
- ICS import/export through the File menu or a `.ics` path argument
  (`qindaqt-calendar file.ics` imports on launch).

## Event edit semantics

`CalendarController::updateEvent()` clones the stored
`KCalendarCore::Event`, applies the full editor field set, bumps the RFC 5545
`REVISION`/`SEQUENCE` and refreshes `LAST-MODIFIED`, and replaces the stored
event through `EventStore::updateEvent()` — never delete+recreate. The event
UID stays stable, so reminder scheduling and occurrence expansion keep their
identity invariants across edits. Recurrence and reminder edits replace the
stored rule/alarms wholesale: an empty recurrence rule clears recurrence, and
a reminder choice of "None" removes all alarms. Rejected edits (empty
summary, end before start, unknown rule) leave the stored event untouched and
surface through `operationFailed`.

For all-day events the end date is inclusive, mirroring
`KCalendarCore::Event::dtEnd()`; the editor round-trips that convention
exactly.

## Bounded exclusions

Calendar is deliberately local and bounded. There is no CalDAV or network
sync (a Milestone 2 sync provider reconciles against the stable RFC 5545
UIDs), no VTODO tasks, no attendees/organizer or scheduling workflow, no
timezone UI (events store local times), and no portal file chooser for
import/export (paths are plain text fields). Reminders fire only while the
application runs; there is no background reminder daemon. Events cannot move
between calendars in the editor.

## Testing and qualification

All rows carry the `calendar` label
(`ctest -L calendar --output-on-failure`):

- `qindaqt.calendar-event-store`, `-collection`, `-occurrence-expander`,
  `-reminder-scheduler`, `-reminder-delivery`: model-layer unit coverage,
  including atomic-persist rollback and bus-absent reminder fallback.
- `qindaqt.calendar-event-update`: uid stability, revision bumps, and
  recurrence/reminder persistence and clearing through
  `CalendarController::updateEvent()`, plus rejection cases and the
  `selectedEvent` details map.
- `qindaqt.calendar-action-catalog`: catalog validity, keyboard completeness,
  and view-action checkable conventions.
- `qindaqt.calendar-ics-import-export`: RFC 5545 round-trips against the
  committed Apple/Google/malformed fixtures.
- `qindaqt.calendar-ui-contract-offscreen`: the production QML exposes the
  required accessible objects (toolbar, views, sidebar, dialogs, reminder
  banner, details pane with Edit/Delete).
- `qindaqt.calendar-ui-actions-offscreen`: drives the production QML through
  create → edit (summary + recurrence + reminder, revision 1) → edit
  (cleared, revision 2) → view switching → delete → fixture import, verifying
  persistence by reloading the store from disk.

The UI rows run offscreen with the software rasterizer, no session bus, and
`QT_FATAL_WARNINGS=1`; the CLI row
(`qindaqt.calendar-cli-rejects-multiple-paths`) and
`qindaqt.calendar-desktop-metadata` cover the launcher contract.

## Packaging

Calendar installs as the `Calendar` CMake component of the
`gui-apps/qindaqt-apps` package (see
[Gentoo app installation](../development/gentoo-apps.md) and
[ADR-0096](../adr/0096-package-bundled-apps-with-portage.md)) and rides the
complete-tree `gui-wm/qindaqt-desktop` package, which declares
`kde-frameworks/kcalendarcore:6`. The earlier prepare-time patch that skipped
the in-flight calendar subdirectory has been dropped.
