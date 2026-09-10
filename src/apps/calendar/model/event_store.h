// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "calendar_types.h"

#include <KCalendarCore/Event>
#include <KCalendarCore/MemoryCalendar>

#include <QHash>
#include <QList>
#include <QString>

namespace QindaQt::Apps::Calendar {

enum class EventStoreError {
  None,
  InvalidCalendarId,
  DuplicateCalendar,
  UnknownCalendar,
  DuplicateEvent,
  UnknownEvent,
  ReadFailed,
  WriteFailed,
  Malformed,
};

struct EventStoreResult final {
  EventStoreError error = EventStoreError::None;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return error == EventStoreError::None; }
};

// One calendar that failed to load during loadAll(). Loading continues for
// the remaining calendars so one malformed .ics never blocks the collection.
struct CalendarLoadFailure final {
  QString calendarId;
  QString diagnostic;

  [[nodiscard]] bool operator==(const CalendarLoadFailure &) const = default;
};

struct EventStoreLoadResult final {
  QList<CalendarLoadFailure> failures;

  [[nodiscard]] bool ok() const { return failures.isEmpty(); }
};

// Persistence authority for event data: one RFC 5545 .ics file per calendar
// beneath an injected root directory (composition selects
// $XDG_DATA_HOME/qindaqt/calendar). Every mutation is persisted immediately
// through an atomic QSaveFile replace, and a failed persist rolls the
// in-memory mutation back, so the store never diverges silently from disk.
// Calendar metadata (names, colors, enabled flags, the default id) is owned
// by CalendarCollection; this store only tracks the calendars it was handed.
//
// AGENT-CONTRACT: Event uids are RFC 5545 UIDs and are the stable identity a
// Milestone 2 sync provider reconciles against. Etag/last-modified/dirty
// semantics are reserved for that provider boundary and deliberately not
// modeled here; do not add per-event sync bookkeeping to this store.
class EventStore final {
public:
  explicit EventStore(QString rootDirectory);

  [[nodiscard]] QString rootDirectory() const;
  [[nodiscard]] QString calendarFilePath(const QString &calendarId) const;

  // A missing root or missing .ics file is a clean first run: the calendar
  // loads empty and no failure is recorded. A malformed file is reported per
  // calendar and that calendar loads empty.
  [[nodiscard]] EventStoreLoadResult loadAll(const QList<CalendarInfo> &calendars);

  // Registers a new empty calendar in memory; the .ics file appears on the
  // first event mutation. Fails with DuplicateCalendar when the id is taken.
  [[nodiscard]] EventStoreResult addCalendar(const CalendarInfo &info);
  // Drops the calendar and deletes its .ics file; an already-absent file is
  // not an error.
  [[nodiscard]] EventStoreResult removeCalendar(const QString &calendarId);
  [[nodiscard]] QList<CalendarInfo> calendars() const;
  [[nodiscard]] bool hasCalendar(const QString &calendarId) const;
  // Null for unknown calendars. The calendar stays owned by the store;
  // callers may read and expand it but must mutate through this class.
  [[nodiscard]] KCalendarCore::MemoryCalendar::Ptr calendar(
      const QString &calendarId) const;

  // The event's uid must be non-empty and unique within the calendar.
  [[nodiscard]] EventStoreResult addEvent(
      const QString &calendarId, const KCalendarCore::Event::Ptr &event);
  // Batch variant for importers: validates the whole list first, then adds
  // and persists once. On any error nothing is added.
  [[nodiscard]] EventStoreResult addEvents(
      const QString &calendarId, const KCalendarCore::Event::List &events);
  // Replaces the stored event that shares the event's uid.
  [[nodiscard]] EventStoreResult updateEvent(
      const QString &calendarId, const KCalendarCore::Event::Ptr &event);
  [[nodiscard]] EventStoreResult removeEvent(const QString &calendarId,
                                             const QString &eventUid);
  [[nodiscard]] KCalendarCore::Event::List events(
      const QString &calendarId) const;

private:
  struct CalendarData {
    CalendarInfo info;
    KCalendarCore::MemoryCalendar::Ptr memoryCalendar;
  };

  [[nodiscard]] static bool isValidCalendarId(const QString &calendarId);
  [[nodiscard]] EventStoreResult persistCalendar(const QString &calendarId);

  QString m_rootDirectory;
  QHash<QString, CalendarData> m_calendars;
};

} // namespace QindaQt::Apps::Calendar
