// SPDX-License-Identifier: LGPL-3.0-or-later

#include "process_model.h"

namespace QindaQt::SystemMonitor {
namespace {

enum Column {
  PidColumn,
  NameColumn,
  CpuColumn,
  MemoryColumn,
  ReadColumn,
  WriteColumn,
  UserColumn,
  StateColumn,
  ThreadsColumn,
  NiceColumn,
  CommandColumn,
  ColumnCount,
};

QVariant optionalNumber(const std::optional<double> value) {
  return value ? QVariant(*value) : QVariant();
}

} // namespace

ProcessModel::ProcessModel(QObject *parent) : QAbstractTableModel(parent) {}

int ProcessModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : int(m_processes.size());
}

int ProcessModel::columnCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : ColumnCount;
}

QVariant ProcessModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 ||
      index.row() >= m_processes.size()) {
    return {};
  }
  const auto &process = m_processes[index.row()];
  const auto &counters = process.counters;
  if (role == Qt::DisplayRole) {
    switch (index.column()) {
    case PidColumn:
      return counters.pid;
    case NameColumn:
      return counters.name;
    case CpuColumn:
      return optionalNumber(process.cpuPercent);
    case MemoryColumn:
      return counters.memoryAvailable
                 ? QVariant::fromValue(counters.memoryBytes)
                 : QVariant();
    case ReadColumn:
      return optionalNumber(process.readRate);
    case WriteColumn:
      return optionalNumber(process.writeRate);
    case UserColumn:
      return counters.user;
    case StateColumn:
      return counters.state;
    case ThreadsColumn:
      return counters.threads;
    case NiceColumn:
      return counters.nice;
    case CommandColumn:
      return counters.command;
    default:
      return {};
    }
  }
  switch (role) {
  case PidRole:
    return counters.pid;
  case StartTicksRole:
    return QVariant::fromValue(counters.startTicks);
  case NameRole:
    return counters.name;
  case CpuRole:
    return optionalNumber(process.cpuPercent);
  case MemoryRole:
    return counters.memoryAvailable ? QVariant::fromValue(counters.memoryBytes)
                                    : QVariant();
  case ReadRateRole:
    return optionalNumber(process.readRate);
  case WriteRateRole:
    return optionalNumber(process.writeRate);
  case UserRole:
    return counters.user;
  case StateRole:
    return counters.state;
  case ThreadsRole:
    return counters.threads;
  case NiceRole:
    return counters.nice;
  case CommandRole:
    return counters.command;
  default:
    return {};
  }
}

QVariant ProcessModel::headerData(int section, Qt::Orientation orientation,
                                  int role) const {
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
    return QAbstractTableModel::headerData(section, orientation, role);
  }
  static const QStringList labels = {
      QStringLiteral("PID"),        QStringLiteral("Process"),
      QStringLiteral("CPU (%)"),    QStringLiteral("Memory (bytes)"),
      QStringLiteral("Read (B/s)"), QStringLiteral("Write (B/s)"),
      QStringLiteral("User"),       QStringLiteral("State"),
      QStringLiteral("Threads"),    QStringLiteral("Nice"),
      QStringLiteral("Command")};
  return labels.value(section);
}

QHash<int, QByteArray> ProcessModel::roleNames() const {
  return {{PidRole, "pid"},
          {StartTicksRole, "startTicks"},
          {NameRole, "name"},
          {CpuRole, "cpu"},
          {MemoryRole, "memory"},
          {ReadRateRole, "readRate"},
          {WriteRateRole, "writeRate"},
          {UserRole, "user"},
          {StateRole, "state"},
          {ThreadsRole, "threads"},
          {NiceRole, "nice"},
          {CommandRole, "command"}};
}

void ProcessModel::replace(QVector<ProcessSample> processes) {
  bool sameIdentities = processes.size() == m_processes.size();
  for (qsizetype i = 0; sameIdentities && i < processes.size(); ++i) {
    sameIdentities =
        processes[i].counters.pid == m_processes[i].counters.pid &&
        processes[i].counters.startTicks == m_processes[i].counters.startTicks;
  }
  if (sameIdentities) {
    m_processes = std::move(processes);
    if (!m_processes.isEmpty()) {
      Q_EMIT dataChanged(index(0, 0),
                         index(int(m_processes.size()) - 1, ColumnCount - 1));
    }
    return;
  }
  beginResetModel();
  m_processes = std::move(processes);
  endResetModel();
}

} // namespace QindaQt::SystemMonitor
