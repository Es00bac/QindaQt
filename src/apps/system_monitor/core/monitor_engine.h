// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QAbstractItemModel>
#include <QObject>
#include <QVariantMap>

namespace QindaQt::SystemMonitor {

/**
 * Asynchronously samples Linux host and process state without blocking its
 * owning thread. The engine and returned model remain owned by the thread that
 * constructs the engine; sampling runs through Qt Concurrent and publishes
 * complete generations back on that thread.
 *
 * Published snapshot contract (after each updated() signal):
 *  - cpu: aggregate busy percentage in [0, 100].
 *  - cores: maps with zero-based integer id and usage percentage in [0, 100].
 *  - memory: total, used, available, cached, swapTotal and swapUsed in bytes.
 *  - disks: major:minor id, kernel name, readRate and writeRate in
 * bytes/second, and busy in percent. Whole devices are reported without their
 * partitions; rates are null until a valid counter interval exists.
 *  - filesystems: path/device plus total, used and available in bytes. Remote
 *    and FUSE mounts are omitted so one unavailable server cannot stall
 * samples.
 *  - network: name, rxRate/txRate in bytes/second and rxBytes/txBytes counters.
 *    Rates are null until a valid counter interval exists.
 *  - uptime: seconds; load1/load5/load15: Linux runnable-queue load averages;
 *    timestamp: UTC milliseconds since the Unix epoch.
 *  - history: at most 300 oldest-to-newest maps. cpu and memory are
 * percentages, diskRead/diskWrite/netRx/netTx are aggregate bytes/second, and
 * timestamp is UTC milliseconds since the Unix epoch. A rate field is null when
 * no valid baseline exists.
 *
 * Process model custom roles are pid, startTicks, name, cpu, memory, readRate,
 * writeRate, user, state, threads, nice and command. startTicks is Linux clock
 * ticks since boot and combines with pid to form the action identity. memory is
 * resident bytes or null when procfs denies it; cpu is percent of total machine
 * capacity; I/O rates are bytes/second or null before a baseline or when procfs
 * denies the counters. The memory cached value is Cached + SReclaimable from
 * Linux meminfo. DisplayRole exposes sensible columns.
 *
 * interval is clamped to 250..10000 ms and defaults to 1000 ms. Pausing stops
 * periodic collection while retaining the last snapshot and history; an
 * explicit requestSample() remains a one-shot refresh. Empty QString from
 * processAction() means success. A non-empty result is the real validation or
 * operating-system error and is also emitted through errorOccurred(). Signals
 * use Linux pidfds and fail closed when identity-bound signaling is
 * unavailable; priority changes use the kernel PID API with checks immediately
 * around it. Supported action names are terminate, kill, stop, continue and
 * nice; nice requires its desired -20..19 value.
 */
class MonitorEngine final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QAbstractItemModel *processes READ processes CONSTANT)
  Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY updated)
  Q_PROPERTY(
      int interval READ interval WRITE setInterval NOTIFY intervalChanged)
  Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)

public:
  explicit MonitorEngine(QObject *parent = nullptr);
  ~MonitorEngine() override;

  [[nodiscard]] QAbstractItemModel *processes() const;
  [[nodiscard]] QVariantMap snapshot() const;
  [[nodiscard]] int interval() const;
  [[nodiscard]] bool paused() const;

  void setInterval(int milliseconds);
  void setPaused(bool paused);
  Q_INVOKABLE void requestSample();
  Q_INVOKABLE QString processAction(qint64 pid, quint64 startTicks,
                                    const QString &action, int value = 0);

Q_SIGNALS:
  void updated();
  void errorOccurred(const QString &message);
  void intervalChanged();
  void pausedChanged();

private:
  class Private;
  Private *const d;
};

} // namespace QindaQt::SystemMonitor
