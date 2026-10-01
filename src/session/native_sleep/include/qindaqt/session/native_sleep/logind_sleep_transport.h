// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QTimer>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <functional>
class QDBusServiceWatcher;
namespace QindaQt::Session::NativeSleep {
enum class SleepResult { Confirmed, Refused, Uncertain };
// AGENT-CONTRACT: same-thread, injected bus and selected identity. Admission is
// readonly and checks supervisor/ordinary-peer lifetime; captures outlive this
// object. Production expects UID 0 for logind. Tests inject a private broker
// and expected fixture UID, never an environment override. Owns one CLOEXEC
// sleep delay FD; stop/revoke/destruction close it and fence every late reply.
class LogindSleepTransport final : public QObject {
  Q_OBJECT
public:
  LogindSleepTransport(QDBusConnection bus, QString selectedSessionId,
                       quint32 sessionUid, quint32 supervisorPid,
                       std::function<bool()> admission, quint32 logindUid = 0,
                       QObject *parent = nullptr);
  ~LogindSleepTransport() override;
  void start();
  void stop();
  bool available() const;
  bool hasDelayInhibitor() const;
  bool preparingForSleep() const { return available() && m_preparing; }
  bool requestSuspend(std::function<bool()> protectedAdmission);
  bool suspendDispatched() const { return m_suspendPending && m_mutationDispatched; }
  bool setLockedHint(bool protectedLocked);
  void releaseDelayInhibitor();
  void acquireDelayInhibitor();
Q_SIGNALS:
  void availabilityChanged();
  void lockRequested();
  void unlockRequested();
  void prepareForSleep(bool preparing);
  void suspendFinished(QindaQt::Session::NativeSleep::SleepResult result);
private Q_SLOTS:
  void receiveLock(const QDBusMessage &message);
  void receiveUnlock(const QDBusMessage &message);
  void receivePrepare(bool preparing, const QDBusMessage &message);
  void receiveRemoved(const QString &id, const QDBusObjectPath &path, const QDBusMessage &message);
private:
  using Completion = std::function<void(const QDBusMessage &)>;
  void call(QString destination, QString path, QString interface,
            QString method, QVariantList arguments, quint64 generation,
            Completion completion);
  void resolve();
  void validateSession(quint64 generation);
  void revoke();
  void closeDelay();
  bool current(quint64 generation) const;
  bool signalAdmitted(const QDBusMessage &message, const QString &path) const;
  QDBusConnection m_bus;
  const QString m_id;
  const quint32 m_uid, m_pid, m_logindUid;
  std::function<bool()> m_admission;
  QDBusServiceWatcher *m_watcher = nullptr;
  QTimer m_lifetime;
  QString m_owner, m_path;
  quint64 m_generation = 0;
  int m_delayFd = -1;
  bool m_started = false, m_ready = false, m_preparing = false, m_preparationSeen = false;
  bool m_acquiring = false, m_suspendPending = false, m_mutationDispatched = false;
};
}

Q_DECLARE_METATYPE(QindaQt::Session::NativeSleep::SleepResult)
