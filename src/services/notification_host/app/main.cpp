// SPDX-License-Identifier: GPL-3.0-or-later

#include "qindaqt/services/notification_host/qt_deadline_scheduler.h"
#include "qindaqt/services/notification_host/resident_notification_host.h"
#include "qindaqt/services/notifications/notification_clock.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QTextStream>

using namespace QindaQt::Services;

namespace {

int failureExitCode(NotificationHost::NotificationHostStartStatus status) {
  using Status = NotificationHost::NotificationHostStartStatus;
  switch (status) {
  case Status::Started:
    return 0;
  case Status::AlreadyRunning:
  case Status::InvalidServiceName:
  case Status::InvalidPolicy:
    return 2;
  case Status::BusUnavailable:
  case Status::BusQueryFailed:
    return 3;
  case Status::NameOwnershipConflict:
    return 4;
  case Status::ServerRegistrationFailed:
  case Status::DeadlineSchedulingFailed:
  case Status::PresentationRegistrationFailed:
    return 5;
  }
  return 5;
}

} // namespace

int main(int argc, char *argv[]) {
  QCoreApplication application(argc, argv);
  QCoreApplication::setApplicationName(
      QStringLiteral("qindaqt-notification-host"));
  QCoreApplication::setApplicationVersion(QStringLiteral(QINDAQT_VERSION));
  QCoreApplication::setOrganizationDomain(QStringLiteral("qindaqt.org"));

  if (application.arguments().contains(QStringLiteral("--help")) ||
      application.arguments().contains(QStringLiteral("-h"))) {
    QTextStream(stdout)
        << "Usage: qindaqt-notification-host [--help] [--version]\n"
        << "Own the freedesktop notification service for this QindaQt "
           "session.\n";
    return 0;
  }
  if (application.arguments().contains(QStringLiteral("--version"))) {
    QTextStream(stdout) << QCoreApplication::applicationName() << ' '
                        << QCoreApplication::applicationVersion() << '\n';
    return 0;
  }

  Notifications::SteadyNotificationClock clock;
  NotificationHost::QtNotificationDeadlineScheduler scheduler;
  Notifications::FreedesktopServerIdentity identity;
  identity.version = QStringLiteral(QINDAQT_VERSION);
  NotificationHost::ResidentNotificationHost host(
      QDBusConnection::sessionBus(), clock, scheduler, {}, identity);
  const auto started = host.start();
  if (!started.ok()) {
    QTextStream(stderr) << QCoreApplication::applicationName() << ": "
                        << NotificationHost::notificationHostStartStatusName(
                               started.status)
                        << ": " << started.message << '\n';
    return failureExitCode(started.status);
  }

  QObject::connect(&application, &QCoreApplication::aboutToQuit, &application,
                   [&host] { host.stop(); });
  return application.exec();
}
