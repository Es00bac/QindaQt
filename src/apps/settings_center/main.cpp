// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings_navigation_controller.h"
#include "settings_route_registry.h"

#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/apps/settings_appearance/appearance_settings_model.h"
#include "qindaqt/apps/settings_appearance/appearance_theme_catalog.h"
#include "qindaqt/apps/settings_appearance/appearance_values.h"
#include "qindaqt/services/settings_client/do_not_disturb_controller.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <QStyleHints>

#include <cstdio>
namespace {

// AGENT-CONTRACT: Installed-theme discovery contract: standard data
// locations first, then the layout beside the installed executable.
[[nodiscard]] QStringList themeSearchDirectories() {
  QStringList directories;
  directories.append(QStandardPaths::locateAll(
      QStandardPaths::GenericDataLocation, QStringLiteral("qindaqt/themes"),
      QStandardPaths::LocateDirectory));
  directories.append(QDir(QCoreApplication::applicationDirPath())
                         .absoluteFilePath(QStringLiteral(
                             QINDAQT_INSTALL_THEME_RELATIVE_PATH)));
  directories.removeDuplicates();
  return directories;
}

void addSettingsQmlImportPaths(QQmlApplicationEngine &engine) {
  const QString applicationDirectory =
      QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
  const QString buildRoot = QStringLiteral(QINDAQT_BUILD_ROOT);
  if (applicationDirectory == buildRoot ||
      applicationDirectory.startsWith(buildRoot + QLatin1Char('/'))) {
    engine.addImportPath(QStringLiteral(QINDAQT_BUILD_QML_IMPORT_PATH));
  }
  engine.addImportPath(
      QDir(applicationDirectory)
          .absoluteFilePath(QStringLiteral(QINDAQT_INSTALL_QML_RELATIVE_PATH)));
}

} // namespace

int main(int argc, char **argv) {
  QGuiApplication application(argc, argv);
  application.setApplicationName(QStringLiteral("qindaqt-settings"));
  application.setOrganizationName(QStringLiteral("QindaQt"));
  // AGENT-CONTRACT: The installed product identity is the desktop entry
  // org.qindaqt.Settings (see org.qindaqt.Settings.desktop beside this
  // file). Wayland window identity and the private readiness harness both
  // derive from this binding; regressing it mislabels every settings
  // window. Set before any window exists, matching the text editor.
  application.setDesktopFileName(QStringLiteral("org.qindaqt.Settings"));

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("QindaQt Settings"));
  parser.addHelpOption();
  const QCommandLineOption pageOption(
      QStringLiteral("page"), QStringLiteral("Open a settings page"),
      QStringLiteral("route"), QStringLiteral("notifications"));
  parser.addOption(pageOption);
  const QCommandLineOption themeDirectoryOption(
      QStringLiteral("theme-directory"),
      QStringLiteral("Additional local theme directory"),
      QStringLiteral("path"));
  parser.addOption(themeDirectoryOption);
  parser.process(application);

  const QString page = parser.value(pageOption);
  const auto registry =
      QindaQt::Apps::SettingsCenter::SettingsRouteRegistry::createDefault();
  if (!registry.hasRoute(page)) {
    std::fprintf(stderr, "qindaqt-settings: unknown page: %s\n",
                 qPrintable(page));
    return 2;
  }

  QQmlApplicationEngine engine;
  // AGENT-GUARD: Build and installed module roots have different layouts.
  // Never add the compiled build path to a relocated installed executable,
  // or a staged package can pass by importing uninstalled developer files.
  addSettingsQmlImportPaths(engine);

  // AGENT-GUARD: Each SettingsClient needs an independent transport. Client
  // request tokens are scoped to one client and begin at the same value; two
  // clients connected to one transport could accept each other's replies.
  // Both transports and both domain models must outlive application.exec().
  QindaQt::Services::SettingsClient::QtSettingsTransport quietingTransport(
      QDBusConnection::sessionBus());
  QindaQt::Services::SettingsClient::SettingsClient quietingClient(
      quietingTransport, {QStringLiteral("services.doNotDisturb")});
  QindaQt::Services::SettingsClient::DoNotDisturbController quieting(
      quietingClient);
  QString quietingError;
  if (!quietingClient.start(&quietingError)) {
    qWarning("qindaqt-settings: Settings1 client unavailable: %s",
             qPrintable(quietingError));
  }

  QStringList directories = themeSearchDirectories();
  const QString explicitThemeDirectory = parser.value(themeDirectoryOption);
  if (!explicitThemeDirectory.isEmpty()) {
    directories.prepend(QFileInfo(explicitThemeDirectory).absoluteFilePath());
  }

  QString catalogError;
  const auto themes =
      QindaQt::Apps::SettingsAppearance::loadAppearanceThemeDirectories(
          directories, &catalogError);
  if (!themes.has_value()) {
    std::fprintf(stderr, "qindaqt-settings: %s\n", qPrintable(catalogError));
    return 3;
  }

  QString facadeError;
  auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
      engine, &facadeError);
  if (facade == nullptr) {
    std::fprintf(stderr, "qindaqt-settings: %s\n", qPrintable(facadeError));
    return 3;
  }

  QindaQt::Services::SettingsClient::QtSettingsTransport appearanceTransport(
      QDBusConnection::sessionBus());
  QindaQt::Services::SettingsClient::SettingsClient appearanceClient(
      appearanceTransport,
      QindaQt::Apps::SettingsAppearance::AppearanceKeys::scopedKeys());
  QindaQt::Apps::SettingsAppearance::AppearanceSettingsModel appearanceSettings(
      appearanceClient, *themes, application.styleHints()->colorScheme(),
      facade);
  QString appearanceClientError;
  if (!appearanceClient.start(&appearanceClientError)) {
    qWarning("qindaqt-settings: Settings1 client unavailable: %s",
             qPrintable(appearanceClientError));
  }

  // AGENT-CONTRACT: Initialize the Settings navigation controller with the
  // requested route.
  QindaQt::Apps::SettingsCenter::SettingsNavigationController navigation(
      registry, page);

  engine.setInitialProperties({
      {QStringLiteral("navigation"),
       QVariant::fromValue(static_cast<QObject *>(&navigation))},
      {QStringLiteral("quietingSettings"),
       QVariant::fromValue(static_cast<QObject *>(&quieting))},
      {QStringLiteral("appearanceSettings"),
       QVariant::fromValue(static_cast<QObject *>(&appearanceSettings))},
  });

  engine.loadFromModule(QStringLiteral("QindaQt.SettingsApp"),
                        QStringLiteral("Main"));
  if (engine.rootObjects().isEmpty()) {
    return 3;
  }

  return application.exec();
}
