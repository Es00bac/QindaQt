// SPDX-License-Identifier: GPL-3.0-or-later

#include "qindaqt/services/notification_host/qt_deadline_scheduler.h"
#include "qindaqt/services/notification_host/resident_notification_host.h"
#include "qindaqt/services/notification_presentation/presentation_token_channel.h"
#include "qindaqt/services/notifications/notification_clock.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QTextStream>

#include <optional>
#include <utility>

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

std::optional<NotificationPresentation::PresentationAccessToken>
presentationToken(QCommandLineParser &parser, QString *error) {
  if (!parser.isSet(QStringLiteral("presentation-token-fd"))) {
    return std::nullopt;
  }
  bool valid = false;
  const int descriptor =
      parser.value(QStringLiteral("presentation-token-fd")).toInt(&valid);
  if (!valid || descriptor < 3) {
    *error = QStringLiteral("presentation token descriptor must be an integer at least 3");
    return std::nullopt;
  }
  auto result = NotificationPresentation::PresentationTokenChannel::readAndClose(
      descriptor);
  if (!result.ok()) {
    *error = std::move(result.message);
    return std::nullopt;
  }
  return std::move(result.token);
}

} // namespace

int main(int argc, char *argv[]) {
  QCoreApplication application(argc, argv);
  QCoreApplication::setApplicationName(
      QStringLiteral("qindaqt-notification-host"));
  QCoreApplication::setApplicationVersion(QStringLiteral(QINDAQT_VERSION));
  QCoreApplication::setOrganizationDomain(QStringLiteral("qindaqt.org"));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("Own the freedesktop notification service for this QindaQt session."));
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addOption(QCommandLineOption(
      QStringLiteral("presentation-token-fd"),
      QStringLiteral("Consume the private presentation token from this inherited descriptor."),
      QStringLiteral("descriptor")));
  parser.process(application);

  QString tokenError;
  auto presentationAccessToken = presentationToken(parser, &tokenError);
  if (parser.isSet(QStringLiteral("presentation-token-fd")) &&
      !presentationAccessToken) {
    QTextStream(stderr) << QCoreApplication::applicationName() << ": "
                        << tokenError << '\n';
    return 2;
  }

  Notifications::SteadyNotificationClock clock;
  NotificationHost::QtNotificationDeadlineScheduler scheduler;
  Notifications::FreedesktopServerIdentity identity;
  identity.version = QStringLiteral(QINDAQT_VERSION);
  NotificationHost::ResidentNotificationHost host(
      QDBusConnection::sessionBus(), clock, scheduler, {}, identity, nullptr,
      std::move(presentationAccessToken));
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
