// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QString>
#include <QVariantMap>
#include <QVector>

#include <optional>

namespace QindaQt::SystemMonitor {

struct CpuCounters final {
  QString id;
  quint64 total = 0;
  quint64 idle = 0;
};

struct DiskCounters final {
  QString id;
  QString name;
  quint64 readBytes = 0;
  quint64 writeBytes = 0;
  quint64 busyMilliseconds = 0;
};

struct NetworkCounters final {
  QString name;
  quint64 rxBytes = 0;
  quint64 txBytes = 0;
};

struct FilesystemSample final {
  QString path;
  QString device;
  quint64 total = 0;
  quint64 used = 0;
  quint64 available = 0;
};

struct ProcessCounters final {
  qint64 pid = 0;
  quint64 startTicks = 0;
  QString name;
  quint64 cpuTicks = 0;
  quint64 memoryBytes = 0;
  bool memoryAvailable = false;
  quint64 readBytes = 0;
  quint64 writeBytes = 0;
  bool readBytesAvailable = false;
  bool writeBytesAvailable = false;
  QString user;
  QString state;
  int threads = 0;
  int nice = 0;
  QString command;
};

struct RawSample final {
  qint64 monotonicNanoseconds = 0;
  qint64 timestampMilliseconds = 0;
  CpuCounters cpu;
  QVector<CpuCounters> cores;
  quint64 memoryTotal = 0;
  quint64 memoryAvailable = 0;
  quint64 memoryCached = 0;
  quint64 swapTotal = 0;
  quint64 swapFree = 0;
  QVector<DiskCounters> disks;
  QVector<FilesystemSample> filesystems;
  QVector<NetworkCounters> network;
  double uptimeSeconds = 0.0;
  double load1 = 0.0;
  double load5 = 0.0;
  double load15 = 0.0;
  QVector<ProcessCounters> processes;
};

struct ProcessSample final {
  ProcessCounters counters;
  std::optional<double> cpuPercent;
  std::optional<double> readRate;
  std::optional<double> writeRate;
};

struct PublishedSample final {
  QVariantMap snapshot;
  QVector<ProcessSample> processes;
};

} // namespace QindaQt::SystemMonitor
