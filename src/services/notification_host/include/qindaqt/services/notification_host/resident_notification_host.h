// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notification_host/deadline_scheduler.h"
#include "qindaqt/services/notifications/freedesktop_notification_server.h"

#include <QDBusConnection>
#include <QString>

#include <memory>

namespace QindaQt::Services::NotificationHost {

enum class NotificationHostStartStatus {
  Started,
  AlreadyRunning,
  InvalidServiceName,
  InvalidPolicy,
  BusUnavailable,
  BusQueryFailed,
  NameOwnershipConflict,
  ServerRegistrationFailed,
  DeadlineSchedulingFailed,
};

struct NotificationHostStartResult final {
  NotificationHostStartStatus status =
      NotificationHostStartStatus::ServerRegistrationFailed;
  QString message;

  [[nodiscard]] bool ok() const noexcept {
    return status == NotificationHostStartStatus::Started;
  }
};

enum class NotificationHostRuntimeStatus {
  Healthy,
  DeadlineSchedulingFailed,
  ExpirationFailed,
};

struct NotificationHostRuntimeState final {
  NotificationHostRuntimeStatus status = NotificationHostRuntimeStatus::Healthy;
  QString message;

  [[nodiscard]] bool healthy() const noexcept {
    return status == NotificationHostRuntimeStatus::Healthy;
  }
};

// Resident composition root for the standard notification server and its one
// expiration deadline. The clock, scheduler, and optional presentation backend
// are borrowed and must outlive the host. All must share the QDBusConnection's
// thread. The host stores its own connection handle and releases its registered
// object and well-known name from stop() and destruction.
class ResidentNotificationHost final {
public:
  ResidentNotificationHost(
      QDBusConnection connection, Notifications::NotificationClock &clock,
      NotificationDeadlineScheduler &scheduler,
      Notifications::NotificationPolicy policy = {},
      Notifications::FreedesktopServerIdentity identity = {},
      Notifications::NotificationBackend *presentationBackend = nullptr);
  ~ResidentNotificationHost();

  ResidentNotificationHost(const ResidentNotificationHost &) = delete;
  ResidentNotificationHost &
  operator=(const ResidentNotificationHost &) = delete;
  ResidentNotificationHost(ResidentNotificationHost &&) = delete;
  ResidentNotificationHost &operator=(ResidentNotificationHost &&) = delete;

  [[nodiscard]] NotificationHostStartResult
  start(const QString &serviceName =
            QStringLiteral("org.freedesktop.Notifications"));
  void stop() noexcept;

  [[nodiscard]] bool isRunning() const noexcept;
  [[nodiscard]] const NotificationHostRuntimeState &
  runtimeState() const noexcept;
  [[nodiscard]] Notifications::NotificationService &service() noexcept;
  [[nodiscard]] const Notifications::NotificationService &
  service() const noexcept;

private:
  class Private;
  std::unique_ptr<Private> d;
};

[[nodiscard]] QString
notificationHostStartStatusName(NotificationHostStartStatus status);

} // namespace QindaQt::Services::NotificationHost
