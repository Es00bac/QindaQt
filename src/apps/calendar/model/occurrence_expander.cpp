// SPDX-License-Identifier: GPL-3.0-or-later
#include "occurrence_expander.h"

#include <KCalendarCore/Alarm>
#include <KCalendarCore/Event>
#include <KCalendarCore/OccurrenceIterator>

#include <QDateTime>
#include <QString>

#include <algorithm>

namespace QindaQt::Apps::Calendar {

QList<Occurrence> expandOccurrences(
    const KCalendarCore::MemoryCalendar &calendar, const QString &calendarId,
    const QDateTime &rangeStart, const QDateTime &rangeEnd) {
  QList<Occurrence> occurrences;
  if (!rangeStart.isValid() || !rangeEnd.isValid() || rangeStart >= rangeEnd) {
    return occurrences;
  }

  for (const KCalendarCore::Event::Ptr &event : calendar.rawEvents()) {
    if (!event) {
      continue;
    }
    KCalendarCore::OccurrenceIterator iterator(calendar, event, rangeStart,
                                               rangeEnd);
    while (iterator.hasNext()) {
      iterator.next();
      const QDateTime start = iterator.occurrenceStartDate();
      if (!start.isValid() || start < rangeStart || start >= rangeEnd) {
        continue;
      }
      occurrences.append({.eventUid = event->uid(),
                          .calendarId = calendarId,
                          .summary = event->summary(),
                          .location = event->location(),
                          .start = start,
                          .end = iterator.occurrenceEndDate(),
                          .allDay = event->allDay(),
                          .recurring = event->recurs()});
    }
  }

  std::sort(occurrences.begin(), occurrences.end(),
            [](const Occurrence &left, const Occurrence &right) {
              if (left.start != right.start) {
                return left.start < right.start;
              }
              if (left.eventUid != right.eventUid) {
                return left.eventUid < right.eventUid;
              }
              return left.summary < right.summary;
            });
  return occurrences;
}

QList<ReminderRequest> expandReminders(
    const KCalendarCore::MemoryCalendar &calendar, const QDateTime &rangeStart,
    const QDateTime &rangeEnd) {
  QList<ReminderRequest> requests;
  if (!rangeStart.isValid() || !rangeEnd.isValid() || rangeStart >= rangeEnd) {
    return requests;
  }

  for (const KCalendarCore::Event::Ptr &event : calendar.rawEvents()) {
    if (!event || event->alarms().isEmpty()) {
      continue;
    }
    KCalendarCore::OccurrenceIterator iterator(calendar, event, rangeStart,
                                               rangeEnd);
    while (iterator.hasNext()) {
      iterator.next();
      const QDateTime start = iterator.occurrenceStartDate();
      if (!start.isValid() || start < rangeStart || start >= rangeEnd) {
        continue;
      }
      for (const KCalendarCore::Alarm::Ptr &alarm : event->alarms()) {
        if (!alarm || !alarm->enabled()) {
          continue;
        }
        QDateTime fireAt;
        if (alarm->hasStartOffset()) {
          fireAt = start.addSecs(alarm->startOffset().asSeconds());
        } else if (alarm->time().isValid()) {
          fireAt = alarm->time();
        }
        if (!fireAt.isValid()) {
          continue;
        }
        requests.append({.eventUid = event->uid(),
                         .summary = event->summary(),
                         .occurrenceStart = start,
                         .fireAt = fireAt});
      }
    }
  }
  return requests;
}

} // namespace QindaQt::Apps::Calendar
