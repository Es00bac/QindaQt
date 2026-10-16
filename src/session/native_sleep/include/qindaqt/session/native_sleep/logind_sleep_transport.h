// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/platform/recovery_backoff/recovery_backoff.h>
#include <qindaqt/session/native_sleep/sleep_mode.h>
#include <QObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <functional>
class QDBusServiceWatcher;
namespace QindaQt::Session::NativeSleep {
enum class SleepResult { Confirmed, Refused, Uncertain };
// Bounded retry of the logind binding (owner/session validation and
// subscriptions) and of the delay inhibitor/locked hint after transient
// errors. Tests inject manual timers.
struct SleepTransportRecovery final {
  Platform::Recovery::BackoffPolicy policy{};
  Platform::Recovery::RecoveryTimerFactory timers{};
};
// AGENT-CONTRACT: same-thread, injected bus and selected identity. Admission is
// readonly, cheap (no bus call) and checks supervisor/ordinary-peer lifetime;
// captures outlive this object. The owner calls reevaluateAdmission() whenever
// an admission input changes; nothing polls it. Production expects UID 0 for
// logind. Tests inject a private broker and expected fixture UID, never an
// environment override. Owns one CLOEXEC sleep delay FD; stop/unbind/
// destruction close it and fence every late reply.
//
// AGENT-CONTRACT (ADR-0379): the logind owner is cached from NameOwnerChanged.
// Subscriptions and the delay inhibitor stay bound across slow replies; a
// transient error retries with bounded backoff, an owner change rebinds and a
// validation mismatch refuses that owner until it changes. Calls address the
// bound unique owner, so a stale cache can only fail, never misdirect.
// availabilityChanged() fires only when available/delay/preparing changes.
class LogindSleepTransport final : public QObject {
  Q_OBJECT
public:
  LogindSleepTransport(QDBusConnection bus, QString selectedSessionId,
                       quint32 sessionUid, quint32 supervisorPid,
                       std::function<bool()> admission, quint32 logindUid = 0,
                       QObject *parent = nullptr);
  LogindSleepTransport(QDBusConnection bus, QString selectedSessionId,
                       quint32 sessionUid, quint32 supervisorPid,
                       std::function<bool()> admission, quint32 logindUid,
                       SleepTransportRecovery recovery,
                       QObject *parent = nullptr);
  ~LogindSleepTransport() override;
  void start();
  void stop();
  bool available() const;
  bool hasDelayInhibitor() const;
  bool preparingForSleep() const { return available() && m_preparing; }
  // Admission withdrawn: release the delay inhibitor (the lock cannot be
  // applied). Admission restored: re-acquire it. Logind subscriptions stay.
  void reevaluateAdmission();
  // Capability is caller-relative to this admitted supervisor connection;
  // completion always runs once, including timeout or retired authority.
  void queryCapability(SleepMode mode, std::function<void(bool)> completion);
  bool requestSleep(SleepMode mode, std::function<bool()> protectedAdmission);
  bool requestSuspend(std::function<bool()> protectedAdmission) {
    return requestSleep(SleepMode::Suspend, std::move(protectedAdmission));
  }
  // Retire the exact request; dispatched work is uncertain and never replayed.
  void cancelSleep();
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
  void bind(const QString &owner);
  void validateSession(quint64 generation);
  void subscribe(quint64 generation);
  void unbind();
  void bindingFailed(quint64 generation, const QString &reason, bool transient);
  void retryMaintenance();
  void sendLockedHint();
  void closeDelay();
  void publishAvailability();
  bool admitted() const { return m_admission && m_admission(); }
  bool current(quint64 generation) const;
  bool signalFromOwner(const QDBusMessage &message, const QString &path) const;
  QDBusConnection m_bus;
  const QString m_id;
  const quint32 m_uid, m_pid, m_logindUid;
  std::function<bool()> m_admission;
  QDBusServiceWatcher *m_watcher = nullptr;
  Platform::Recovery::RecoveryBackoff m_bindRetry, m_maintenanceRetry;
  QString m_owner, m_path;
  quint64 m_generation = 0, m_requestSerial = 0;
  int m_delayFd = -1;
  bool m_started = false, m_ready = false, m_preparing = false, m_preparationSeen = false;
  bool m_acquiring = false, m_suspendPending = false, m_mutationDispatched = false;
  bool m_sessionRemoved = false;
  // Desired/confirmed logind LockedHint; resent on change, rebind or failure.
  bool m_desiredHint = false, m_hasDesiredHint = false, m_hintConfirmed = false;
  bool m_hintPending = false, m_confirmedHint = false;
  // Last published (available, delay, preparing) snapshot.
  int m_published = 0;
};
}

Q_DECLARE_METATYPE(QindaQt::Session::NativeSleep::SleepResult)
