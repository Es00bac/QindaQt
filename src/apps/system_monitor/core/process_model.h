// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "sample_types.h"

#include <QAbstractTableModel>

namespace QindaQt::SystemMonitor {

class ProcessModel final : public QAbstractTableModel {
  Q_OBJECT

public:
  enum Role {
    PidRole = Qt::UserRole + 1,
    StartTicksRole,
    NameRole,
    CpuRole,
    MemoryRole,
    ReadRateRole,
    WriteRateRole,
    UserRole,
    StateRole,
    ThreadsRole,
    NiceRole,
    CommandRole,
  };

  explicit ProcessModel(QObject *parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
  [[nodiscard]] int columnCount(const QModelIndex &parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex &index,
                              int role = Qt::DisplayRole) const override;
  [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                    int role = Qt::DisplayRole) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  void replace(QVector<ProcessSample> processes);

private:
  QVector<ProcessSample> m_processes;
};

} // namespace QindaQt::SystemMonitor
