// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/monitor_engine.h"
#include "hardware/hardware_sampler.h"
#include "ui/monitor_controller.h"
#include "ui/system_monitor_appearance.h"
#include "ui/system_monitor_window.h"

#include "qindaqt/app_appearance/application_appearance_controller.h"
#include "qindaqt/services/font_discovery/font_session_bootstrap.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/themes/theme_loader.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QJsonDocument>
#include <QPointer>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

#include <cstdio>
#include <functional>
#include <memory>

namespace {

QStringList themeSearchDirectories(const QString &explicitDirectory) {
  QStringList directories;
  if (!explicitDirectory.isEmpty()) {
    directories.append(QFileInfo(explicitDirectory).absoluteFilePath());
  }
  directories.append(QStandardPaths::locateAll(
      QStandardPaths::GenericDataLocation, QStringLiteral("qindaqt/themes"),
      QStandardPaths::LocateDirectory));
  directories.append(
      QDir(QCoreApplication::applicationDirPath())
          .absoluteFilePath(QStringLiteral("../share/qindaqt/themes")));
  directories.removeDuplicates();
  return directories;
}

QindaQt::Themes::LoadResult loadTheme(const QString &themeId,
                                      const QStringList &directories) {
  for (const QString &directory : directories) {
    const QString path =
        QDir(directory).filePath(themeId + QStringLiteral(".json"));
    if (QFileInfo::exists(path)) {
      return QindaQt::Themes::ThemeLoader::fromFile(path);
    }
  }
  return {.ok = false,
          .theme = {},
          .error = QStringLiteral("Theme '%1' was not found").arg(themeId)};
}

} // namespace

int main(int argc, char *argv[]) {
  QindaQt::Services::FontDiscovery::FontSessionBootstrap::
      applyFromSessionSettings();
  QApplication application(argc, argv);
  application.setApplicationName(QStringLiteral("QindaQt System Monitor"));
  application.setApplicationDisplayName(
      QStringLiteral("QindaQt System Monitor"));
  application.setDesktopFileName(QStringLiteral("org.qindaqt.SystemMonitor"));
  QIcon icon = QIcon::fromTheme(QStringLiteral("org.qindaqt.SystemMonitor"));
  if (icon.isNull()) {
    icon = QIcon::fromTheme(QStringLiteral("utilities-system-monitor"));
  }
  application.setWindowIcon(icon);

  QCommandLineParser parser;
  parser.setApplicationDescription(QObject::tr("QindaQt System Monitor"));
  parser.addHelpOption();
  const QCommandLineOption themeOption(
      QStringLiteral("theme"), QObject::tr("QindaQt theme identifier."),
      QObject::tr("id"), QStringLiteral("qinda-dark"));
  const QCommandLineOption themeDirectoryOption(
      QStringLiteral("theme-directory"),
      QObject::tr("Additional theme directory."), QObject::tr("path"));
  const QCommandLineOption viewOption(
      QStringLiteral("view"), QObject::tr("Open one view in this window."),
      QObject::tr("view"));
  const QCommandLineOption quitOption(
      QStringLiteral("quit-after"),
      QObject::tr("Quit after this many milliseconds."),
      QObject::tr("milliseconds"));
  const QCommandLineOption sampleOption(
      QStringLiteral("sample"),
      QObject::tr("Write one monitor snapshot as JSON and exit."));
  parser.addOption(themeOption);
  parser.addOption(themeDirectoryOption);
  parser.addOption(viewOption);
  parser.addOption(quitOption);
  parser.addOption(sampleOption);
  parser.process(application);

  const auto initialTheme =
      loadTheme(parser.value(themeOption),
                themeSearchDirectories(parser.value(themeDirectoryOption)));
  if (!initialTheme.ok) {
    std::fprintf(stderr, "qindaqt-system-monitor: %s\n",
                 qPrintable(initialTheme.error));
    return 3;
  }
  const auto initialAppearance =
      QindaQt::Apps::SystemMonitor::SystemMonitorAppearanceAdapter::fromTheme(
          initialTheme.theme);
  if (!initialAppearance.ok()) {
    std::fprintf(stderr, "qindaqt-system-monitor: %s\n",
                 qPrintable(initialAppearance.diagnostic));
    return 3;
  }
  application.setPalette(initialAppearance.appearance->palette);
  application.setFont(initialAppearance.appearance->interfaceFont);

  QindaQt::Services::SettingsClient::QtSettingsTransport appearanceTransport(
      QDBusConnection::sessionBus());
  QindaQt::Services::SettingsClient::SettingsClient appearanceClient(
      appearanceTransport, {QStringLiteral("appearance.theme"),
                            QStringLiteral("appearance.colorScheme")});
  QindaQt::AppAppearance::ApplicationAppearanceController appearanceController(
      appearanceClient,
      QindaQt::AppAppearance::standardThemeDirectories(
          parser.value(themeDirectoryOption)),
      QStringLiteral("qinda-dark"),
      parser.isSet(themeOption) ? parser.value(themeOption) : QString());
  const auto applyAppearance = [&application, &appearanceController] {
    const auto adapted =
        QindaQt::Apps::SystemMonitor::SystemMonitorAppearanceAdapter::fromTheme(
            appearanceController.theme());
    if (adapted.ok()) {
      application.setPalette(adapted.appearance->palette);
      application.setFont(adapted.appearance->interfaceFont);
    }
  };
  QObject::connect(&appearanceController,
                   &QindaQt::AppAppearance::ApplicationAppearanceController::
                       appearanceChanged,
                   &application, applyAppearance);
  applyAppearance();
  QString appearanceError;
  if (!appearanceClient.start(&appearanceError)) {
    std::fprintf(
        stderr,
        "qindaqt-system-monitor: appearance settings unavailable (%s)\n",
        qPrintable(appearanceError));
  }

  QindaQt::SystemMonitor::MonitorEngine engine;
  if (parser.isSet(sampleOption)) {
    QObject::connect(
        &engine, &QindaQt::SystemMonitor::MonitorEngine::updated, &application,
        [&engine, &application] {
          QTextStream(stdout)
              << QJsonDocument::fromVariant(engine.snapshot()).toJson();
          application.quit();
        });
    engine.requestSample();
    return application.exec();
  }

  const auto hardware =
      std::make_shared<QindaQt::SystemMonitor::HardwareSampler>();
  QindaQt::Apps::SystemMonitor::MonitorController controller(
      engine, [hardware] { return hardware->sample(); });
  QList<QPointer<QindaQt::Apps::SystemMonitor::SystemMonitorWindow>> windows;
  std::function<void(const QString &)> openWindow;
  openWindow = [&](const QString &view) {
    auto *window =
        new QindaQt::Apps::SystemMonitor::SystemMonitorWindow(controller);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->setInitialView(view);
    windows.append(window);
    static int menuConnectionSerial = 0;
    const QString connectionName =
        QStringLiteral("qindaqt-system-monitor-%1-%2")
            .arg(QCoreApplication::applicationPid())
            .arg(++menuConnectionSerial);
    QObject::connect(
        window,
        &QindaQt::Apps::SystemMonitor::SystemMonitorWindow::openViewRequested,
        &application, [&openWindow](const QString &requestedView) {
          openWindow(requestedView);
        });
    QObject::connect(window, &QObject::destroyed, &application,
                     [&windows, &application, connectionName] {
                       QDBusConnection::disconnectFromBus(connectionName);
                       windows.removeAll(nullptr);
                       if (windows.isEmpty()) {
                         application.quit();
                       }
                     });
    window->show();
    const QDBusConnection menuConnection = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, connectionName);
    if (menuConnection.isConnected()) {
      window->composeMenuExport(menuConnection);
    }
  };
  openWindow(parser.value(viewOption));
  if (parser.isSet(quitOption)) {
    QTimer::singleShot(parser.value(quitOption).toInt(), &application,
                       &QCoreApplication::quit);
  }
  const int result = application.exec();
  const auto remainingWindows = windows;
  for (const QPointer<QindaQt::Apps::SystemMonitor::SystemMonitorWindow>
           &window : remainingWindows) {
    if (window) {
      window->setAttribute(Qt::WA_DeleteOnClose, false);
      delete window.data();
    }
  }
  return result;
}
