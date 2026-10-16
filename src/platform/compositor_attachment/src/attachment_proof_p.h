// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QDBusMessage>
#include <QObject>
#include <QPointer>
#include <functional>
class QDBusPendingCallWatcher;
namespace QindaQt::Platform::Compositor::Private {
// Bus-daemon facts for one compositor owner and one selected session owner.
// Every query names a unique connection or the daemon itself, so a stale
// answer can only fail a later step; it can never redirect authority.
struct ProofFacts final {
  QString compositorOwner;
  quint64 compositorPid = 0;
};
// Retry: no owner yet, a slow/failed daemon reply or a refused candidate.
// Retire: the selected session owner itself is gone or foreign.
enum class ProofOutcome { Proven, Retry, Retire };
struct ProofResult final {
  ProofOutcome outcome = ProofOutcome::Retry;
  ProofFacts facts;
  QString reason;
};
// Blocking proof for attach(); each daemon call has its own explicit bound and
// never changes the connection's shared interface timeout.
ProofResult proveSynchronously(const QDBusConnection &bus,
                               const QString &sessionOwner, int timeoutMs);

// Same proof as a chain of bounded asynchronous calls for automatic
// re-attachment. Completion runs at most once, on this object's thread, and
// never after cancel(). The owner may delete this object only via deleteLater.
class AsyncProof final : public QObject {
  Q_OBJECT
public:
  using Completion = std::function<void(const ProofResult &)>;
  AsyncProof(QDBusConnection bus, QString sessionOwner, int timeoutMs,
             Completion completion, QObject *parent = nullptr);
  ~AsyncProof() override;
  void start();
  void cancel();

private:
  void issue();
  void finish(const ProofResult &result);
  QDBusConnection m_bus;
  QString m_sessionOwner;
  int m_timeoutMs;
  Completion m_completion;
  QPointer<QDBusPendingCallWatcher> m_pending;
  ProofFacts m_facts;
  int m_step = 0;
  bool m_done = false;
};
} // namespace QindaQt::Platform::Compositor::Private
