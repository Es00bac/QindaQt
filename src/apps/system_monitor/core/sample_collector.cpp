// SPDX-License-Identifier: LGPL-3.0-or-later

#include "sample_collector.h"

#include <QHash>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <unistd.h>

namespace QindaQt::SystemMonitor {
namespace {

QVariant numberOrNull(const std::optional<double> value) {
  return value ? QVariant(*value) : QVariant::fromValue(nullptr);
}

std::optional<double> counterRate(quint64 current, quint64 previous,
                                  double seconds) {
  if (seconds <= 0.0 || current < previous) {
    return std::nullopt;
  }
  return double(current - previous) / seconds;
}

double cpuUsage(const CpuCounters &current, const CpuCounters *previous) {
  quint64 total = current.total;
  quint64 idle = current.idle;
  if (previous && current.total >= previous->total &&
      current.idle >= previous->idle) {
    total -= previous->total;
    idle -= previous->idle;
  }
  if (total == 0 || idle > total) {
    return 0.0;
  }
  return std::clamp(100.0 * double(total - idle) / double(total), 0.0, 100.0);
}

QVariantMap memoryMap(const RawSample &sample) {
  const quint64 used = sample.memoryTotal >= sample.memoryAvailable
                           ? sample.memoryTotal - sample.memoryAvailable
                           : 0;
  const quint64 swapUsed = sample.swapTotal >= sample.swapFree
                               ? sample.swapTotal - sample.swapFree
                               : 0;
  return {{QStringLiteral("total"), QVariant::fromValue(sample.memoryTotal)},
          {QStringLiteral("used"), QVariant::fromValue(used)},
          {QStringLiteral("available"),
           QVariant::fromValue(sample.memoryAvailable)},
          {QStringLiteral("cached"), QVariant::fromValue(sample.memoryCached)},
          {QStringLiteral("swapTotal"), QVariant::fromValue(sample.swapTotal)},
          {QStringLiteral("swapUsed"), QVariant::fromValue(swapUsed)}};
}

QVariantList projectCores(const RawSample &current, const RawSample *previous) {
  QHash<QString, CpuCounters> baselines;
  if (previous) {
    for (const auto &core : previous->cores) {
      baselines.insert(core.id, core);
    }
  }
  QVariantList result;
  for (const auto &core : current.cores) {
    const auto found = baselines.constFind(core.id);
    const CpuCounters *baseline =
        found == baselines.cend() ? nullptr : &found.value();
    result.push_back(
        QVariantMap{{QStringLiteral("id"), core.id.toInt()},
                    {QStringLiteral("usage"), cpuUsage(core, baseline)}});
  }
  return result;
}

struct RateProjection final {
  QVariantList items;
  std::optional<double> incoming;
  std::optional<double> outgoing;
};

RateProjection projectDisks(const RawSample &current, const RawSample *previous,
                            double elapsed) {
  QHash<QString, DiskCounters> baselines;
  if (previous) {
    for (const auto &disk : previous->disks) {
      baselines.insert(disk.id, disk);
    }
  }
  RateProjection result;
  for (const auto &disk : current.disks) {
    std::optional<double> readRate;
    std::optional<double> writeRate;
    std::optional<double> busy;
    const auto found = baselines.constFind(disk.id);
    if (found != baselines.cend()) {
      readRate = counterRate(disk.readBytes, found->readBytes, elapsed);
      writeRate = counterRate(disk.writeBytes, found->writeBytes, elapsed);
      const auto busyRate = counterRate(
          disk.busyMilliseconds, found->busyMilliseconds, elapsed * 1000.0);
      if (busyRate) {
        busy = std::clamp(*busyRate * 100.0, 0.0, 100.0);
      }
    }
    if (readRate) {
      result.incoming = result.incoming.value_or(0.0) + *readRate;
    }
    if (writeRate) {
      result.outgoing = result.outgoing.value_or(0.0) + *writeRate;
    }
    result.items.push_back(
        QVariantMap{{QStringLiteral("id"), disk.id},
                    {QStringLiteral("name"), disk.name},
                    {QStringLiteral("readRate"), numberOrNull(readRate)},
                    {QStringLiteral("writeRate"), numberOrNull(writeRate)},
                    {QStringLiteral("busy"), numberOrNull(busy)}});
  }
  return result;
}

RateProjection projectNetwork(const RawSample &current,
                              const RawSample *previous, double elapsed) {
  QHash<QString, NetworkCounters> baselines;
  if (previous) {
    for (const auto &interface : previous->network) {
      baselines.insert(interface.name, interface);
    }
  }
  RateProjection result;
  for (const auto &interface : current.network) {
    std::optional<double> rxRate;
    std::optional<double> txRate;
    const auto found = baselines.constFind(interface.name);
    if (found != baselines.cend()) {
      rxRate = counterRate(interface.rxBytes, found->rxBytes, elapsed);
      txRate = counterRate(interface.txBytes, found->txBytes, elapsed);
    }
    if (rxRate) {
      result.incoming = result.incoming.value_or(0.0) + *rxRate;
    }
    if (txRate) {
      result.outgoing = result.outgoing.value_or(0.0) + *txRate;
    }
    result.items.push_back(QVariantMap{
        {QStringLiteral("name"), interface.name},
        {QStringLiteral("rxRate"), numberOrNull(rxRate)},
        {QStringLiteral("txRate"), numberOrNull(txRate)},
        {QStringLiteral("rxBytes"), QVariant::fromValue(interface.rxBytes)},
        {QStringLiteral("txBytes"), QVariant::fromValue(interface.txBytes)},
    });
  }
  return result;
}

QVariantList projectFilesystems(const RawSample &current) {
  QVariantList result;
  for (const auto &filesystem : current.filesystems) {
    result.push_back(QVariantMap{
        {QStringLiteral("path"), filesystem.path},
        {QStringLiteral("device"), filesystem.device},
        {QStringLiteral("total"), QVariant::fromValue(filesystem.total)},
        {QStringLiteral("used"), QVariant::fromValue(filesystem.used)},
        {QStringLiteral("available"),
         QVariant::fromValue(filesystem.available)},
    });
  }
  return result;
}

QVector<ProcessSample> projectProcesses(const RawSample &current,
                                        const RawSample *previous,
                                        double elapsed) {
  QHash<QString, ProcessCounters> baselines;
  if (previous) {
    for (const auto &process : previous->processes) {
      baselines.insert(QString::number(process.pid) + QLatin1Char(':') +
                           QString::number(process.startTicks),
                       process);
    }
  }
  const long ticksPerSecond = std::max(1L, sysconf(_SC_CLK_TCK));
  const int capacity = std::max(1, int(current.cores.size()));
  QVector<ProcessSample> result;
  result.reserve(current.processes.size());
  for (const auto &process : current.processes) {
    ProcessSample published;
    published.counters = process;
    const QString identity = QString::number(process.pid) + QLatin1Char(':') +
                             QString::number(process.startTicks);
    const auto found = baselines.constFind(identity);
    if (found != baselines.cend()) {
      const auto cpuRate =
          counterRate(process.cpuTicks, found->cpuTicks, elapsed);
      if (cpuRate) {
        published.cpuPercent = std::clamp(
            100.0 * *cpuRate / double(ticksPerSecond * capacity), 0.0, 100.0);
      }
      if (process.readBytesAvailable && found->readBytesAvailable) {
        published.readRate =
            counterRate(process.readBytes, found->readBytes, elapsed);
      }
      if (process.writeBytesAvailable && found->writeBytesAvailable) {
        published.writeRate =
            counterRate(process.writeBytes, found->writeBytes, elapsed);
      }
    }
    result.push_back(std::move(published));
  }
  std::sort(result.begin(), result.end(),
            [](const auto &left, const auto &right) {
              return left.counters.pid < right.counters.pid;
            });
  return result;
}

} // namespace

SampleCollector::SampleCollector(QString procRoot)
    : m_reader(std::move(procRoot)) {}

PublishedSample SampleCollector::collect(QString *error) {
  QString localError;
  QString *outputError = error ? error : &localError;
  RawSample current = m_reader.read(outputError);
  if (!outputError->isEmpty()) {
    return {};
  }
  return publish(std::move(current));
}

PublishedSample SampleCollector::publish(RawSample current) {
  const RawSample *previous = m_previous ? &*m_previous : nullptr;
  const double elapsed = previous ? double(current.monotonicNanoseconds -
                                           previous->monotonicNanoseconds) /
                                        1'000'000'000.0
                                  : 0.0;

  const QVariantList cores = projectCores(current, previous);
  const RateProjection disks = projectDisks(current, previous, elapsed);
  const RateProjection network = projectNetwork(current, previous, elapsed);
  const QVariantList filesystems = projectFilesystems(current);
  QVector<ProcessSample> processes =
      projectProcesses(current, previous, elapsed);

  const double aggregateCpu =
      cpuUsage(current.cpu, previous ? &previous->cpu : nullptr);
  const quint64 memoryUsed = current.memoryTotal >= current.memoryAvailable
                                 ? current.memoryTotal - current.memoryAvailable
                                 : 0;
  const double memoryPercent =
      current.memoryTotal > 0
          ? 100.0 * double(memoryUsed) / double(current.memoryTotal)
          : 0.0;
  QVariantMap historyPoint{
      {QStringLiteral("cpu"), aggregateCpu},
      {QStringLiteral("memory"), std::clamp(memoryPercent, 0.0, 100.0)},
      {QStringLiteral("diskRead"), numberOrNull(disks.incoming)},
      {QStringLiteral("diskWrite"), numberOrNull(disks.outgoing)},
      {QStringLiteral("netRx"), numberOrNull(network.incoming)},
      {QStringLiteral("netTx"), numberOrNull(network.outgoing)},
      {QStringLiteral("timestamp"), current.timestampMilliseconds},
  };
  m_history.push_back(historyPoint);
  while (m_history.size() > 300) {
    m_history.removeFirst();
  }

  PublishedSample result;
  result.snapshot = {
      {QStringLiteral("cpu"), aggregateCpu},
      {QStringLiteral("cores"), cores},
      {QStringLiteral("memory"), memoryMap(current)},
      {QStringLiteral("disks"), disks.items},
      {QStringLiteral("filesystems"), filesystems},
      {QStringLiteral("network"), network.items},
      {QStringLiteral("uptime"), current.uptimeSeconds},
      {QStringLiteral("load1"), current.load1},
      {QStringLiteral("load5"), current.load5},
      {QStringLiteral("load15"), current.load15},
      {QStringLiteral("timestamp"), current.timestampMilliseconds},
      {QStringLiteral("history"), m_history},
  };
  result.processes = std::move(processes);
  m_previous = std::move(current);
  return result;
}

void SampleCollector::reset() {
  m_previous.reset();
  m_history.clear();
}

} // namespace QindaQt::SystemMonitor
