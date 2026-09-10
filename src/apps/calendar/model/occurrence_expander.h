// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "calendar_types.h"

#include <KCalendarCore/MemoryCalendar>

#include <QList>

class QDateTime;
class QString;

namespace QindaQt::Apps::Calendar {

// Expands every event of the calendar into concrete occurrences whose start
// falls inside [rangeStart, rangeEnd), sorted by start (then uid, then
// summary) for a stable presentation order. RRULE/RDATE/EXDATE handling is
// delegated to KCalendarCore::OccurrenceIterator, whose end bound is
// INCLUSIVE — this function filters start >= rangeEnd out to honor the
// half-open contract. An event starting before rangeStart but overlapping
// the window is not reported; views that need overlap must widen the range
// themselves.
//
// Pure function: no QObject, no globals, no persistence.
[[nodiscard]] QList<Occurrence> expandOccurrences(
    const KCalendarCore::MemoryCalendar &calendar, const QString &calendarId,
    const QDateTime &rangeStart, const QDateTime &rangeEnd);

// Expands reminder requests for the same window: for every occurrence of an
// event carrying enabled alarms, one ReminderRequest per alarm. Relative
// triggers fire at occurrenceStart + startOffset (VALARM offsets are
// typically negative); absolute TRIGGER;VALUE=DATE-TIME alarms fire at their
// recorded time. Audio/procedure/email alarms are included — the notifier
// boundary decides presentation; only disabled alarms are skipped.
[[nodiscard]] QList<ReminderRequest> expandReminders(
    const KCalendarCore::MemoryCalendar &calendar, const QDateTime &rangeStart,
    const QDateTime &rangeEnd);

} // namespace QindaQt::Apps::Calendar
