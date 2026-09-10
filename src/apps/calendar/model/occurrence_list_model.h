// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "calendar_types.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QVariantList>

namespace QindaQt::Apps::Calendar {

// Flat read-only model over the occurrences of the currently visible range.
// CalendarController recomputes the contents via resetOccurrences() whenever
// the view range, the calendar set, or the store changes; the model itself
// holds no KCalendarCore state.
class OccurrenceListModel final : public QAbstractListModel {
  Q_OBJECT

public:
  enum Role {
    EventUidRole = Qt::UserRole + 1,
    CalendarIdRole,
    SummaryRole,
    LocationRole,
    StartRole,
    EndRole,
    AllDayRole,
    RecurringRole,
    DayKeyRole,
    CalendarColorTokenRole,
  };
  Q_ENUM(Role)

  explicit OccurrenceListModel(QObject *parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex &parent) const override;
  [[nodiscard]] QVariant data(const QModelIndex &index,
                              int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  void resetOccurrences(const QList<Occurrence> &occurrences,
                        const QHash<QString, QString> &colorTokenByCalendarId);

  // QML day-cell filtering without a proxy model: occurrences whose start
  // date matches the "yyyy-MM-dd" day key.
  [[nodiscard]] Q_INVOKABLE QVariantList
  occurrencesForDay(const QString &dayKey) const;

  [[nodiscard]] const Occurrence *findByUid(const QString &eventUid) const;

private:
  [[nodiscard]] QVariantMap toMap(const Occurrence &occurrence) const;

  QList<Occurrence> m_occurrences;
  QHash<QString, QString> m_colorTokenByCalendarId;
};

} // namespace QindaQt::Apps::Calendar
