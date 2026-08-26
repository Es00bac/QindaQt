// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/notification_host/resident_notification_host.h"

#include "dbus_service_name_validation_p.h"
#include "qindaqt/services/notification_host/notification_presentation_server.h"

#include <QDBusConnectionInterface>
#include <QDBusReply>

#include <functional>
#include <utility>

namespace QindaQt::Services::NotificationHost {
namespace {

using namespace QindaQt::Services::Notifications;

class HostNotificationBackend final : public NotificationBackend {
public:
  using PublicationCallback = std::function<void(NotificationSnapshotPtr)>;

  HostNotificationBackend(PublicationCallback publicationCallback,
                          NotificationBackend *presentationBackend)
      : m_publicationCallback(std::move(publicationCallback)),
        m_presentationBackend(presentationBackend) {}

  void modelPublished(NotificationSnapshotPtr snapshot) override {
    if (m_publicationCallback) {
      m_publicationCallback(snapshot);
    }
    if (m_presentationBackend != nullptr) {
      m_presentationBackend->modelPublished(std::move(snapshot));
    }
  }

  void notificationClosed(const NotificationCloseEvent &event) override {
    if (m_presentationBackend != nullptr) {
      m_presentationBackend->notificationClosed(event);
    }
  }

  void actionInvoked(const NotificationActionEvent &event) override {
    if (m_presentationBackend != nullptr) {
      m_presentationBackend->actionInvoked(event);
    }
  }

private:
  PublicationCallback m_publicationCallback;
  NotificationBackend *m_presentationBackend = nullptr;
};

NotificationHostStartResult startFailure(NotificationHostStartStatus status,
                                         QString message) {
  return {status, std::move(message)};
}

} // namespace

class ResidentNotificationHost::Private final {
public:
  Private(QDBusConnection selectedConnection, NotificationClock &selectedClock,
          NotificationDeadlineScheduler &selectedScheduler,
          NotificationPolicy policy, FreedesktopServerIdentity identity,
          NotificationBackend *presentationBackend,
          std::optional<NotificationPresentation::PresentationAccessToken>
              presentationAccessToken)
      : connection(std::move(selectedConnection)), clock(selectedClock),
        scheduler(selectedScheduler),
        backend([this](NotificationSnapshotPtr snapshot) {
          modelPublished(std::move(snapshot));
        }, presentationBackend),
        server(connection, clock, std::move(policy), std::move(identity),
               &backend) {
    if (presentationAccessToken) {
      presentation = std::make_unique<NotificationPresentationServer>(
          connection, server.service(), std::move(*presentationAccessToken));
    }
  }

  void modelPublished(NotificationSnapshotPtr snapshot) {
    if (running) {
      const bool scheduled = reconcileDeadline();
      Q_UNUSED(scheduled)
      if (presentation != nullptr && snapshot) {
        presentation->publishRevision(snapshot->revision);
      }
    }
  }

  bool reconcileDeadline() {
    const auto deadline = server.service().nextExpiryDeadlineMs();
    if (!deadline.has_value()) {
      if (deadlineArmed) {
        scheduler.cancel();
        deadlineArmed = false;
      }
      runtimeState = {};
      return true;
    }

    const qint64 now = clock.nowMs();
    if (now < 0) {
      deadlineArmed = false;
      scheduler.cancel();
      runtimeState = {
          NotificationHostRuntimeStatus::DeadlineSchedulingFailed,
          QStringLiteral("notification clock is outside its monotonic domain"),
      };
      return false;
    }
    const qint64 delay = *deadline <= now ? 0 : *deadline - now;
    const auto result =
        scheduler.armAfter(delay, [this] { deadlineReached(); });
    if (!result.ok()) {
      deadlineArmed = false;
      scheduler.cancel();
      runtimeState = {
          NotificationHostRuntimeStatus::DeadlineSchedulingFailed,
          result.message.isEmpty()
              ? QStringLiteral(
                    "notification deadline scheduler rejected the arm")
              : result.message,
      };
      return false;
    }
    deadlineArmed = true;
    runtimeState = {};
    return true;
  }

  void deadlineReached() {
    deadlineArmed = false;
    if (!running) {
      return;
    }

    const quint64 revisionBefore = server.service().snapshot()->revision;
    const auto result = server.service().expireDue();
    if (!result.ok()) {
      scheduler.cancel();
      runtimeState = {
          NotificationHostRuntimeStatus::ExpirationFailed,
          result.message.isEmpty()
              ? QStringLiteral("notification expiration failed")
              : result.message,
      };
      return;
    }

    // A mutation publishes through modelPublished(), which already rearms.
    // An early event-loop wake changes no revision and needs an explicit
    // rearm for the remaining portion of the same absolute deadline.
    if (server.service().snapshot()->revision == revisionBefore) {
      const bool scheduled = reconcileDeadline();
      Q_UNUSED(scheduled)
    }
  }

  QDBusConnection connection;
  NotificationClock &clock;
  NotificationDeadlineScheduler &scheduler;
  HostNotificationBackend backend;
  FreedesktopNotificationServer server;
  std::unique_ptr<NotificationPresentationServer> presentation;
  NotificationHostRuntimeState runtimeState;
  bool running = false;
  bool deadlineArmed = false;
};

ResidentNotificationHost::ResidentNotificationHost(
    QDBusConnection connection, Notifications::NotificationClock &clock,
    NotificationDeadlineScheduler &scheduler,
    Notifications::NotificationPolicy policy,
    Notifications::FreedesktopServerIdentity identity,
    Notifications::NotificationBackend *presentationBackend,
    std::optional<NotificationPresentation::PresentationAccessToken>
        presentationAccessToken)
    : d(std::make_unique<Private>(std::move(connection), clock, scheduler,
                                  std::move(policy), std::move(identity),
                                  presentationBackend,
                                  std::move(presentationAccessToken))) {}

ResidentNotificationHost::~ResidentNotificationHost() { stop(); }

NotificationHostStartResult
ResidentNotificationHost::start(const QString &serviceName) {
  if (d->running) {
    return startFailure(NotificationHostStartStatus::AlreadyRunning,
                        QStringLiteral("notification host is already running"));
  }
  QString serviceNameError;
  if (!QindaQt::Services::NotificationHost::Private::validateWellKnownServiceName(
          serviceName, &serviceNameError)) {
    return startFailure(
        NotificationHostStartStatus::InvalidServiceName,
        std::move(serviceNameError));
  }
  if (!d->server.service().isReady()) {
    return startFailure(NotificationHostStartStatus::InvalidPolicy,
                        d->server.service().initializationError());
  }
  if (!d->connection.isConnected() || d->connection.interface() == nullptr) {
    return startFailure(
        NotificationHostStartStatus::BusUnavailable,
        QStringLiteral("notification session bus is unavailable"));
  }

  const QDBusReply<bool> existing =
      d->connection.interface()->isServiceRegistered(serviceName);
  if (!existing.isValid()) {
    return startFailure(NotificationHostStartStatus::BusQueryFailed,
                        existing.error().message());
  }
  if (existing.value()) {
    return startFailure(
        NotificationHostStartStatus::NameOwnershipConflict,
        QStringLiteral("notification service name is already owned: %1")
            .arg(serviceName));
  }

  if (!d->server.start(serviceName)) {
    const QDBusReply<bool> afterFailure =
        d->connection.interface()->isServiceRegistered(serviceName);
    if (afterFailure.isValid() && afterFailure.value()) {
      return startFailure(
          NotificationHostStartStatus::NameOwnershipConflict,
          QStringLiteral(
              "notification service name was acquired concurrently: %1")
              .arg(serviceName));
    }
    return startFailure(NotificationHostStartStatus::ServerRegistrationFailed,
                        d->server.lastError());
  }

  if (d->presentation != nullptr && !d->presentation->start()) {
    const QString message = d->presentation->lastError();
    d->server.stop();
    return startFailure(
        NotificationHostStartStatus::PresentationRegistrationFailed,
        message.isEmpty()
            ? QStringLiteral("notification presentation registration failed")
            : message);
  }

  d->running = true;
  if (!d->reconcileDeadline()) {
    const QString message = d->runtimeState.message;
    stop();
    return startFailure(NotificationHostStartStatus::DeadlineSchedulingFailed,
                        message);
  }
  return {NotificationHostStartStatus::Started, {}};
}

void ResidentNotificationHost::stop() noexcept {
  if (!d->running && !d->deadlineArmed && !d->server.isRunning()) {
    return;
  }
  d->running = false;
  d->deadlineArmed = false;
  d->scheduler.cancel();
  if (d->presentation != nullptr) {
    d->presentation->stop();
  }
  d->server.stop();
}

bool ResidentNotificationHost::isRunning() const noexcept {
  return d->running && d->server.isRunning();
}

const NotificationHostRuntimeState &
ResidentNotificationHost::runtimeState() const noexcept {
  return d->runtimeState;
}

Notifications::NotificationService &
ResidentNotificationHost::service() noexcept {
  return d->server.service();
}

const Notifications::NotificationService &
ResidentNotificationHost::service() const noexcept {
  return d->server.service();
}

QString notificationHostStartStatusName(NotificationHostStartStatus status) {
  switch (status) {
  case NotificationHostStartStatus::Started:
    return QStringLiteral("started");
  case NotificationHostStartStatus::AlreadyRunning:
    return QStringLiteral("already-running");
  case NotificationHostStartStatus::InvalidServiceName:
    return QStringLiteral("invalid-service-name");
  case NotificationHostStartStatus::InvalidPolicy:
    return QStringLiteral("invalid-policy");
  case NotificationHostStartStatus::BusUnavailable:
    return QStringLiteral("bus-unavailable");
  case NotificationHostStartStatus::BusQueryFailed:
    return QStringLiteral("bus-query-failed");
  case NotificationHostStartStatus::NameOwnershipConflict:
    return QStringLiteral("name-ownership-conflict");
  case NotificationHostStartStatus::ServerRegistrationFailed:
    return QStringLiteral("server-registration-failed");
  case NotificationHostStartStatus::DeadlineSchedulingFailed:
    return QStringLiteral("deadline-scheduling-failed");
  case NotificationHostStartStatus::PresentationRegistrationFailed:
    return QStringLiteral("presentation-registration-failed");
  }
  return QStringLiteral("unknown");
}

} // namespace QindaQt::Services::NotificationHost
