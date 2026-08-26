// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/services/notification_host/deadline_scheduler.h"
#include "qindaqt/services/notifications/notification_backend.h"
#include "qindaqt/services/notifications/notification_clock.h"
#include "qindaqt/services/notifications/notification_types.h"

#include <QProcess>
#include <QStringList>
#include <QUuid>

#include <utility>

namespace QindaQt::Services::NotificationHost::TestSupport {

class PrivateSessionBus final {
public:
  ~PrivateSessionBus() { stop(); }

  [[nodiscard]] bool start(QString *error) {
    m_process.start(QStringLiteral("dbus-daemon"),
                    {QStringLiteral("--session"), QStringLiteral("--nofork"),
                     QStringLiteral("--nopidfile"),
                     QStringLiteral("--print-address=1")});
    if (!m_process.waitForStarted(5'000) ||
        !m_process.waitForReadyRead(5'000)) {
      *error = m_process.errorString();
      return false;
    }
    m_address = QString::fromUtf8(m_process.readLine()).trimmed();
    if (m_address.isEmpty()) {
      *error = QStringLiteral("private dbus-daemon did not publish an address");
      return false;
    }
    return true;
  }

  void stop() noexcept {
    if (m_process.state() == QProcess::NotRunning) {
      return;
    }
    m_process.terminate();
    if (!m_process.waitForFinished(1'000)) {
      m_process.kill();
      m_process.waitForFinished(1'000);
    }
  }

  [[nodiscard]] const QString &address() const noexcept { return m_address; }
  [[nodiscard]] bool isStopped() const noexcept {
    return m_process.state() == QProcess::NotRunning;
  }

private:
  QProcess m_process;
  QString m_address;
};

class ManualNotificationClock final : public Notifications::NotificationClock {
public:
  [[nodiscard]] qint64 nowMs() const noexcept override { return now; }

  qint64 now = 0;
};

class ManualDeadlineScheduler final : public NotificationDeadlineScheduler {
public:
  [[nodiscard]] DeadlineArmResult armAfter(qint64 delayMs,
                                           Callback callback) override {
    armDelays.push_back(delayMs);
    if (rejectNextArm) {
      rejectNextArm = false;
      m_callback = {};
      return {
          DeadlineArmStatus::Rejected,
          QStringLiteral("injected deadline scheduler rejection"),
      };
    }
    m_callback = std::move(callback);
    return {DeadlineArmStatus::Armed, {}};
  }

  void cancel() noexcept override {
    ++cancelCount;
    m_callback = {};
  }

  [[nodiscard]] bool fire() {
    auto callback = std::move(m_callback);
    m_callback = {};
    if (!callback) {
      return false;
    }
    callback();
    return true;
  }

  [[nodiscard]] bool isArmed() const noexcept { return bool(m_callback); }

  QVector<qint64> armDelays;
  int cancelCount = 0;
  bool rejectNextArm = false;

private:
  Callback m_callback;
};

class RecordingNotificationBackend final
    : public Notifications::NotificationBackend {
public:
  void
  modelPublished(Notifications::NotificationSnapshotPtr snapshot) override {
    publications.push_back(std::move(snapshot));
  }

  void notificationClosed(
      const Notifications::NotificationCloseEvent &event) override {
    closures.push_back(event);
  }

  void
  actionInvoked(const Notifications::NotificationActionEvent &event) override {
    actions.push_back(event);
  }

  QVector<Notifications::NotificationSnapshotPtr> publications;
  QVector<Notifications::NotificationCloseEvent> closures;
  QVector<Notifications::NotificationActionEvent> actions;
};

inline Notifications::NotificationRequest request(QString owner,
                                                  int timeoutMs) {
  Notifications::NotificationRequest value;
  value.sourceService = std::move(owner);
  value.applicationName = QStringLiteral("Host Test");
  value.summary = QStringLiteral("Scheduled notification");
  value.body = QStringLiteral("Body");
  value.expireTimeoutMs = timeoutMs;
  return value;
}

inline QString connectionName(const QString &role) {
  return QStringLiteral("qindaqt-notification-host-%1-%2")
      .arg(role, QUuid::createUuid().toString(QUuid::Id128));
}

inline QString serviceName() {
  return QStringLiteral("org.qindaqt.NotificationHostTest.s%1")
      .arg(QUuid::createUuid().toString(QUuid::Id128));
}

} // namespace QindaQt::Services::NotificationHost::TestSupport
