// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QThread>
#include <QVariantMap>
#include <atomic>
#include <memory>
#include <functional>

namespace QindaQt::Apps::FileManager {
// GUI-thread owner of ONE QtCore worker, never one thread/device. Requests and
// results are copied observations; callers fence public attachment/navigation.
// No storage creation/mount/service access. Cancellation supersedes queued
// generations and is checked between roots, not inside kernel filesystem I/O.
// At most one worker invocation and one bounded latest pending value are held;
// frequent public snapshots cannot enqueue an unbounded backlog.
// Destruction invalidates requests, quits and joins before context destruction;
// it does not claim a kernel syscall is interruptible or detach a live worker.
class TrashDiscovery final : public QObject {
  Q_OBJECT
public:
  static constexpr qsizetype maximumRoots = 256;
  using Probe = std::function<QStringList(const QString &)>;
  // Probe is owned, worker-confined and nonthrowing; empty uses owning reader.
  explicit TrashDiscovery(QObject *parent = nullptr, Probe probe = {});
  ~TrashDiscovery() override;
  void request(quint64 generation, QStringList mountedLocalRoots);
Q_SIGNALS:
  void completed(quint64 generation, const QVariantMap &paths, const QString &diagnostic);
private:
  void dispatch();
  QStringList m_pendingRoots;
  QString m_pendingDiagnostic;
  quint64 m_pendingGeneration = 0;
  bool m_running = false;
  Probe m_probe;
  QThread m_thread;
  QObject *m_context;
  std::shared_ptr<std::atomic<quint64>> m_generation;
};
}
