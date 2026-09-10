// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QString>

namespace QindaQt::Apps::Calendar {

// Metadata for one user-visible calendar. `id` is the stable storage key: it
// names the per-calendar .ics file and calendars.json entry, so it must stay
// path-safe (see EventStore's identifier validation). `colorToken` references
// a QST-1 semantic token, never a literal color, so QML stays theme-driven.
struct CalendarInfo final {
  QString id;
  QString displayName;
  QString colorToken;
  bool enabled = true;

  [[nodiscard]] bool operator==(const CalendarInfo &) const = default;
};

// One concrete instance of an event inside an expansion window. Recurring
// events yield one Occurrence per instance, all sharing `eventUid`; the pair
// (eventUid, start) identifies an instance. Domain events stay
// KCalendarCore::Event::Ptr inside the store; this light value crosses to
// presentation so QML/controllers never touch KCalendarCore.
//
// AGENT-NOTE: For all-day occurrences `end` is INCLUSIVE, mirroring
// KCalendarCore::Event::dtEnd() (RFC 5545 DTEND is exclusive; KCalendarCore
// subtracts one day on parse). Presentation code must add one day when
// computing the displayed span of an all-day occurrence.
struct Occurrence final {
  QString eventUid;
  QString calendarId;
  QString summary;
  QString location;
  QDateTime start;
  QDateTime end;
  bool allDay = false;
  bool recurring = false;

  [[nodiscard]] bool operator==(const Occurrence &) const = default;
};

// One pending reminder derived from a VALARM. `fireAt` is the absolute
// delivery time (occurrence start plus the alarm's start offset, or the
// alarm's absolute trigger time).
struct ReminderRequest final {
  QString eventUid;
  QString summary;
  QDateTime occurrenceStart;
  QDateTime fireAt;

  [[nodiscard]] bool operator==(const ReminderRequest &) const = default;
};

} // namespace QindaQt::Apps::Calendar
