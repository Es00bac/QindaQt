// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "calendar_types.h"

#include <QList>
#include <QString>

namespace QindaQt::Apps::Calendar {

enum class CalendarCollectionError {
  None,
  Absent,
  InvalidCalendarId,
  DuplicateCalendar,
  UnknownCalendar,
  CannotRemoveDefaultCalendar,
  ReadFailed,
  WriteFailed,
  Malformed,
};

struct CalendarCollectionResult final {
  CalendarCollectionError error = CalendarCollectionError::None;
  QString diagnostic;

  [[nodiscard]] bool ok() const {
    return error == CalendarCollectionError::None;
  }
};

// Owns the set of calendars (metadata only: id, display name, color token,
// enabled flag) plus the default calendar id. Persists to calendars.json in
// the injected root directory — the same root EventStore writes .ics files
// into. Event payloads are EventStore's job; this class never parses .ics.
// Every mutation is persisted immediately (atomic QSaveFile replace) and a
// failed persist rolls the in-memory change back.
class CalendarCollection final {
public:
  // Id and display name of the calendar created on first run. The Settings1
  // key services.calendarDefaultCalendar defaults to this id.
  static constexpr auto defaultCalendarIdValue = "personal";

  explicit CalendarCollection(QString rootDirectory);

  [[nodiscard]] QString rootDirectory() const;
  [[nodiscard]] QString filePath() const;

  // Absent is a clean first run: the default "Personal" calendar is created
  // in memory, ok() is false, and diagnostic is empty (the caller shows no
  // error). A malformed file leaves the in-memory set untouched.
  [[nodiscard]] CalendarCollectionResult load();

  [[nodiscard]] QList<CalendarInfo> calendars() const;
  [[nodiscard]] QString defaultCalendarId() const;

  // Persists the current in-memory set. Used by composition after a
  // first-run load so the default calendar reaches calendars.json before any
  // other mutation.
  [[nodiscard]] CalendarCollectionResult save() const;

  [[nodiscard]] CalendarCollectionResult addCalendar(const CalendarInfo &info);
  // The default calendar cannot be removed; move the default first.
  [[nodiscard]] CalendarCollectionResult removeCalendar(
      const QString &calendarId);
  [[nodiscard]] CalendarCollectionResult renameCalendar(
      const QString &calendarId, const QString &displayName);
  [[nodiscard]] CalendarCollectionResult setEnabled(const QString &calendarId,
                                                    bool enabled);
  [[nodiscard]] CalendarCollectionResult setDefaultCalendarId(
      const QString &calendarId);

private:
  [[nodiscard]] static bool isValidCalendarId(const QString &calendarId);
  [[nodiscard]] int indexOf(const QString &calendarId) const;
  [[nodiscard]] CalendarCollectionResult persist() const;

  QString m_rootDirectory;
  QList<CalendarInfo> m_calendars;
  QString m_defaultCalendarId;
};

} // namespace QindaQt::Apps::Calendar
