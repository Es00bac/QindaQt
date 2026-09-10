// SPDX-License-Identifier: GPL-3.0-or-later
#include "occurrence_list_model.h"

namespace QindaQt::Apps::Calendar {

OccurrenceListModel::OccurrenceListModel(QObject *parent)
    : QAbstractListModel(parent) {}

int OccurrenceListModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : static_cast<int>(m_occurrences.size());
}

QVariantMap OccurrenceListModel::toMap(const Occurrence &occurrence) const {
  return {
      {QStringLiteral("eventUid"), occurrence.eventUid},
      {QStringLiteral("calendarId"), occurrence.calendarId},
      {QStringLiteral("summary"), occurrence.summary},
      {QStringLiteral("location"), occurrence.location},
      {QStringLiteral("start"), occurrence.start},
      {QStringLiteral("end"), occurrence.end},
      {QStringLiteral("allDay"), occurrence.allDay},
      {QStringLiteral("recurring"), occurrence.recurring},
      {QStringLiteral("dayKey"),
       occurrence.start.date().toString(QStringLiteral("yyyy-MM-dd"))},
      {QStringLiteral("calendarColorToken"),
       m_colorTokenByCalendarId.value(occurrence.calendarId)},
  };
}

QVariant OccurrenceListModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 ||
      index.row() >= m_occurrences.size()) {
    return {};
  }
  const Occurrence &occurrence = m_occurrences.at(index.row());
  switch (role) {
  case EventUidRole:
    return occurrence.eventUid;
  case CalendarIdRole:
    return occurrence.calendarId;
  case SummaryRole:
    return occurrence.summary;
  case LocationRole:
    return occurrence.location;
  case StartRole:
    return occurrence.start;
  case EndRole:
    return occurrence.end;
  case AllDayRole:
    return occurrence.allDay;
  case RecurringRole:
    return occurrence.recurring;
  case DayKeyRole:
    return occurrence.start.date().toString(QStringLiteral("yyyy-MM-dd"));
  case CalendarColorTokenRole:
    return m_colorTokenByCalendarId.value(occurrence.calendarId);
  default:
    return {};
  }
}

QHash<int, QByteArray> OccurrenceListModel::roleNames() const {
  return {
      {EventUidRole, QByteArrayLiteral("eventUid")},
      {CalendarIdRole, QByteArrayLiteral("calendarId")},
      {SummaryRole, QByteArrayLiteral("summary")},
      {LocationRole, QByteArrayLiteral("location")},
      {StartRole, QByteArrayLiteral("start")},
      {EndRole, QByteArrayLiteral("end")},
      {AllDayRole, QByteArrayLiteral("allDay")},
      {RecurringRole, QByteArrayLiteral("recurring")},
      {DayKeyRole, QByteArrayLiteral("dayKey")},
      {CalendarColorTokenRole, QByteArrayLiteral("calendarColorToken")},
  };
}

void OccurrenceListModel::resetOccurrences(
    const QList<Occurrence> &occurrences,
    const QHash<QString, QString> &colorTokenByCalendarId) {
  beginResetModel();
  m_occurrences = occurrences;
  m_colorTokenByCalendarId = colorTokenByCalendarId;
  endResetModel();
}

QVariantList
OccurrenceListModel::occurrencesForDay(const QString &dayKey) const {
  QVariantList result;
  for (const Occurrence &occurrence : m_occurrences) {
    if (occurrence.start.date().toString(QStringLiteral("yyyy-MM-dd")) ==
        dayKey) {
      result.append(toMap(occurrence));
    }
  }
  return result;
}

const Occurrence *
OccurrenceListModel::findByUid(const QString &eventUid) const {
  for (const Occurrence &occurrence : m_occurrences) {
    if (occurrence.eventUid == eventUid) {
      return &occurrence;
    }
  }
  return nullptr;
}

} // namespace QindaQt::Apps::Calendar
