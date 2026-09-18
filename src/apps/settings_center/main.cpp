// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings_navigation_controller.h"
#include "settings_route_registry.h"

#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/apps/settings_appearance/appearance_settings_model.h"
#include "qindaqt/apps/settings_appearance/appearance_theme_catalog.h"
#include "qindaqt/apps/settings_appearance/appearance_values.h"
#include "qindaqt/apps/settings_appearance/wallpaper_catalog.h"
#include "qindaqt/apps/settings_appearance/window_decoration_controller.h"
#include "qindaqt/apps/settings_accessibility/accessibility_settings_model.h"
#include "qindaqt/apps/settings_accessibility/accessibility_values.h"
#include "qindaqt/apps/settings_audio/audio_settings_model.h"
#include "qindaqt/apps/settings_bluetooth/bluetooth_settings_model.h"
#include "qindaqt/apps/settings_display/display_settings_model.h"
#include "qindaqt/apps/settings_network/network_settings_model.h"
#include "qindaqt/services/audio_client/audio_client.h"
#include "qindaqt/services/audio_client/qt_audio_transport.h"
#include "qindaqt/services/bluetooth_client/bluetooth_client.h"
#include "qindaqt/services/bluetooth_client/qt_bluetooth_transport.h"
#include "qindaqt/services/display_client/client.h"
#include "qindaqt/services/display_client/display_coordinator.h"
#include "qindaqt/services/display_client/qt_display_transport.h"
#include "qindaqt/services/font_discovery/font_session_bootstrap.h"
#include "qindaqt/services/network_qt_transport/qt_network_transport.h"
#include "qindaqt/services/settings_client/do_not_disturb_controller.h"
#include "notification_schedule_model.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/shell/icons/icon_runtime.h"

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <QStyleHints>

#include <cstdio>
#include <memory>
namespace {

// AGENT-CONTRACT: Installed-theme discovery contract: standard data
// locations first, then the layout beside the installed executable.
[[nodiscard]] QStringList wallpaperSearchDirectories() {
  QStringList directories = QStandardPaths::locateAll(
      QStandardPaths::GenericDataLocation, QStringLiteral("qindaqt/wallpapers"),
      QStandardPaths::LocateDirectory);
  directories.append(QDir(QCoreApplication::applicationDirPath())
                         .absoluteFilePath(QStringLiteral(
                             QINDAQT_INSTALL_WALLPAPER_RELATIVE_PATH)));
  directories.removeDuplicates();
  return directories;
}

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

// Confined icon-theme roots for the Customize route's preview glyphs: the
// standard per-user and system data locations first, then the relocated
// layout beside an installed executable, and the source catalog only for the
// exact build executable (same guard as the developer QML import path).
[[nodiscard]] QStringList iconThemeRoots() {
  QStringList roots = QindaQt::Shell::Icons::IconRuntime::freedesktopIconRoots(
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation),
      QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation));
  roots.append(QDir(QCoreApplication::applicationDirPath())
                   .absoluteFilePath(QStringLiteral(
                       QINDAQT_INSTALL_ICON_RELATIVE_PATH)));
  const QFileInfo applicationFile(QCoreApplication::applicationFilePath());
  const QString applicationPath = applicationFile.canonicalFilePath();
  const QString buildExecutablePath =
      QFileInfo(QStringLiteral(QINDAQT_BUILD_EXECUTABLE_PATH))
          .canonicalFilePath();
  if (!buildExecutablePath.isEmpty() &&
      applicationPath == buildExecutablePath) {
    roots.append(QStringLiteral(QINDAQT_SOURCE_ICON_DIRECTORY));
  }
  roots.removeDuplicates();
  return roots;
}

// Resolves the QML root this exact binary is allowed to import its own
// QindaQt modules from: the developer tree only for the exact build
// executable, otherwise the binary-relative install prefix.
[[nodiscard]] QString ownQmlRoot() {
  const QFileInfo applicationFile(QCoreApplication::applicationFilePath());
  const QString applicationDirectory = applicationFile.absolutePath();
  const QString applicationPath = applicationFile.canonicalFilePath();
  const QString buildExecutablePath =
      QFileInfo(QStringLiteral(QINDAQT_BUILD_EXECUTABLE_PATH))
          .canonicalFilePath();
  // AGENT-GUARD: Only the exact build executable may import the developer QML
  // tree. Installed test stages deliberately live beneath the build root; a
  // directory-prefix test lets those relocated copies borrow missing modules
  // and turns package verification into a false positive.
  if (!buildExecutablePath.isEmpty() &&
      applicationPath == buildExecutablePath) {
    return QStringLiteral(QINDAQT_BUILD_QML_IMPORT_PATH);
  }
  return QDir(applicationDirectory)
      .absoluteFilePath(QStringLiteral(QINDAQT_INSTALL_QML_RELATIVE_PATH));
}

void addSettingsQmlImportPaths(QQmlApplicationEngine &engine) {
  engine.addImportPath(ownQmlRoot());
}

// AGENT-CONTRACT: every first-party module Main.qml imports must exist in
// this binary's own QML root. The QML engine also searches the Qt install's
// default import path, so without this preflight a relocated or partially
// installed copy silently borrows a same-named module from the system
// package — the exact condition the installed-route poisons must fail on.
// A missing module fails closed with the same exit the import failure would
// produce, before any engine work.
[[nodiscard]] bool ownSettingsModulesPresent(const QString &qmlRoot,
                                             QString *missing) {
  // Exactly the route modules the executable does not statically embed and
  // must therefore resolve as directories from its own prefix. The Customize
  // and *Backend modules are static libraries linked into this binary (like
  // the stage-closure's embedded-module list), so they need no directory.
  static const char *const requiredModules[] = {
      "QindaQt/SettingsApp/Appearance",   "QindaQt/SettingsApp/Display",
      "QindaQt/SettingsApp/Network",
      "QindaQt/SettingsApp/Audio",        "QindaQt/SettingsApp/Bluetooth",
      "QindaQt/SettingsApp/Power",        "QindaQt/SettingsApp/Clipboard",
      "QindaQt/SettingsApp/Color",        "QindaQt/SettingsApp/Accessibility",
      "QindaQt/SettingsApp/Input",   "QindaQt/SettingsApp/Streaming",
      "QindaQt/SettingsApp/Windows",
  };
  for (const char *module : requiredModules) {
    const QString directory = QDir(qmlRoot).filePath(QString::fromLatin1(module));
    if (!QFileInfo::exists(directory)) {
      if (missing != nullptr) {
        *missing = QString::fromLatin1(module) + QStringLiteral(" (root ")
                   + qmlRoot + QLatin1Char(')');
      }
      return false;
    }
  }
  return true;
}

[[nodiscard]] QStringList resolveThemeDirectories(
    const QString &explicitThemeDirectory) {
  QStringList directories = themeSearchDirectories();
  if (!explicitThemeDirectory.isEmpty()) {
    directories.prepend(QFileInfo(explicitThemeDirectory).absoluteFilePath());
  }
  return directories;
}

void startSettingsClient(
    QindaQt::Services::SettingsClient::SettingsClient &client) {
  QString error;
  if (!client.start(&error)) {
    qWarning("qindaqt-settings: Settings1 client unavailable: %s",
             qPrintable(error));
  }
}

// KWin decoration plugin/theme selection is native compositor configuration,
// not a Settings1 appearance value. This factory keeps the platform mutation
// behind its controller while the composition root owns the required lifetime
// (ADR-0160).
[[nodiscard]] std::unique_ptr<
    QindaQt::Apps::SettingsAppearance::WindowDecorationController>
makeWindowDecorationController() {
  return std::make_unique<
      QindaQt::Apps::SettingsAppearance::WindowDecorationController>(
      QindaQt::Apps::SettingsAppearance::windowDecorationConfigPath(),
      QindaQt::Apps::SettingsAppearance::windowDecorationThemeRoots(),
      QindaQt::Apps::SettingsAppearance::requestKWinDecorationReconfigure);
}

// AGENT-CONTRACT: Accessibility owns one independent Settings1 transport and
// a client scoped to exactly the four consumed accessibility keys
// (ADR-0128). Sharing a transport with another Settings1 client is unsafe
// because request tokens are per-client sequences that start alike. Members
// are declared transport-first: each object holds its dependency by
// reference, so reverse declaration order would construct a dangling client.
struct AccessibilityServices {
  QindaQt::Services::SettingsClient::QtSettingsTransport transport;
  QindaQt::Services::SettingsClient::SettingsClient client;
  QindaQt::Apps::SettingsAccessibility::AccessibilitySettingsModel model;

  explicit AccessibilityServices(const QDBusConnection &bus)
      : transport(bus),
        client(transport, QindaQt::Apps::SettingsAccessibility::
                              AccessibilityKeys::scopedKeys()),
        model(client) {
    startSettingsClient(client);
  }
};


// What the command line asked for. Kept as one value so main() stays a
// composition root rather than an argument parser.
struct LaunchArguments {
  QString page;
  QString destination;
  QString selection;
  QString themeDirectory;
};

// AGENT-CONTRACT: `--destination` names a tab within the page and `--select`
// an item to select there, for example
//   qindaqt-settings --page input --destination tablet --select <group>
// Both are opaque here: the route page decides whether it knows them, so a
// stale selection opens the route rather than failing to open anything. They
// are bounded so a hostile argument cannot become an unbounded property.
LaunchArguments parseLaunchArguments(const QCoreApplication &application) {
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
  const QCommandLineOption destinationOption(
      QStringLiteral("destination"),
      QStringLiteral("Open a destination within the page"),
      QStringLiteral("destination"));
  parser.addOption(destinationOption);
  const QCommandLineOption selectOption(
      QStringLiteral("select"),
      QStringLiteral("Select this device or item within the destination"),
      QStringLiteral("id"));
  parser.addOption(selectOption);
  parser.process(application);

  return LaunchArguments{
      parser.value(pageOption),
      parser.value(destinationOption).left(64),
      parser.value(selectOption).left(256),
      parser.value(themeDirectoryOption),
  };
}

// AGENT-CONTRACT: Notifications owns one independent Settings1 transport and
// one client scoped to the four `services.doNotDisturb*` keys. The switch and
// the quiet-hours schedule (ADR-0212) are two projections of that one client,
// so a schedule edit and a Do Not Disturb edit share an owner and a token
// sequence instead of racing two. All four are schema keys with defaults, so
// widening the scope by three cannot turn a present value into
// "unavailable". Members are declared transport-first for the same reason
// AccessibilityServices is: each holds its dependency by reference.
struct QuietingServices {
  QindaQt::Services::SettingsClient::QtSettingsTransport transport;
  QindaQt::Services::SettingsClient::SettingsClient client;
  QindaQt::Services::SettingsClient::DoNotDisturbController controller;
  QindaQt::Apps::SettingsNotifications::NotificationScheduleModel schedule;

  explicit QuietingServices(const QDBusConnection &bus)
      : transport(bus),
        client(transport, {QStringLiteral("services.doNotDisturb"),
                           QStringLiteral("services.doNotDisturbSchedule"),
                           QStringLiteral("services.doNotDisturbStartMinutes"),
                           QStringLiteral("services.doNotDisturbEndMinutes")}),
        controller(client), schedule(client) {
    startSettingsClient(client);
  }
};
} // namespace

int main(int argc, char **argv) {
  // AGENT-CONTRACT: F1 font bootstrap — the single guarded composition-root
  // call runs before QGuiApplication construction (pre-construction
  // QGuiApplication::setFont persists as the application default font). A
  // missing, unavailable, or unresolvable preference source leaves platform
  // defaults untouched (fail-closed). See
  // docs/wiki/architecture/font-preferences.md.
  QindaQt::Services::FontDiscovery::FontSessionBootstrap::applyFromSessionSettings();
  // AGENT-CONTRACT: Settings hosts the widgets application class (a
  // QGuiApplication subclass) only so the Appearance route can paint the
  // real Fusion QStyle in its toolkit preview (ADR-0127). No widget window
  // is ever created; every surface stays QML on QindaQt.Controls.
  QApplication application(argc, argv);
  application.setApplicationName(QStringLiteral("qindaqt-settings"));
  application.setOrganizationName(QStringLiteral("QindaQt"));
  // AGENT-CONTRACT: The installed product identity is the desktop entry
  // org.qindaqt.Settings (see org.qindaqt.Settings.desktop beside this
  // file). Wayland window identity and the private readiness harness both
  // derive from this binding; regressing it mislabels every settings
  // window. Set before any window exists, matching the text editor.
  application.setDesktopFileName(QStringLiteral("org.qindaqt.Settings"));

  const LaunchArguments arguments = parseLaunchArguments(application);
  const QString &page = arguments.page;
  const auto registry =
      QindaQt::Apps::SettingsCenter::SettingsRouteRegistry::createDefault();
  if (!registry.hasRoute(page)) {
    std::fprintf(stderr, "qindaqt-settings: unknown page: %s\n",
                 qPrintable(page));
    return 2;
  }

  QString missingModule;
  if (!ownSettingsModulesPresent(ownQmlRoot(), &missingModule)) {
    std::fprintf(stderr, "qindaqt-settings: required Settings module missing: %s\n",
                 qPrintable(missingModule));
    return 3;
  }

  QQmlApplicationEngine engine;
  // AGENT-GUARD: Build and installed module roots have different layouts.
  // Never add the compiled build path to a relocated installed executable,
  // or a staged package can pass by importing uninstalled developer files.
  addSettingsQmlImportPaths(engine);

  // The Customize route renders real applet glyphs through the confined
  // public icon module. Install the provider before any route component
  // exists; a failed install degrades those glyphs to typed placeholders
  // and never blocks Settings.
  const bool iconsInstalled = QindaQt::Shell::Icons::IconRuntime::install(
      engine, iconThemeRoots(), {QStringLiteral("QindaQt")});
  Q_UNUSED(iconsInstalled)

  QuietingServices quieting(QDBusConnection::sessionBus());

  QStringList directories =
      resolveThemeDirectories(arguments.themeDirectory);

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
  auto windowDecorationSettings = makeWindowDecorationController();
  QindaQt::Services::SettingsClient::QtSettingsTransport appearanceTransport(
      QDBusConnection::sessionBus());
  QindaQt::Services::SettingsClient::SettingsClient appearanceClient(
      appearanceTransport,
      QindaQt::Apps::SettingsAppearance::AppearanceKeys::scopedKeys());
  QindaQt::Apps::SettingsAppearance::AppearanceSettingsModel appearanceSettings(
      appearanceClient, *themes,
      QindaQt::Apps::SettingsAppearance::discoverBundledWallpapers(
          wallpaperSearchDirectories()),
      application.styleHints()->colorScheme(), facade);
  startSettingsClient(appearanceClient);

  QindaQt::DisplayClient::QtDisplayTransport displayTransport(
      QDBusConnection::sessionBus());
  QindaQt::DisplayClient::Client displayClient(&displayTransport);
  QindaQt::DisplayClient::Coordinator displayCoordinator(&displayClient);
  QindaQt::Apps::SettingsDisplay::DisplaySettingsModel displaySettings(
      displayClient, displayCoordinator);
  displayClient.start();

  // AGENT-CONTRACT: The Settings route consumes only the public Network1
  // client/transport boundary. It never links the resident service or libnm
  // adapter and cannot acquire credentials or arbitrary connection settings.
  QindaQt::Network::Client::QtNetworkTransport networkTransport(
      QDBusConnection::sessionBus());
  QindaQt::Network::Client::NetworkClient networkClient(networkTransport);
  QindaQt::Apps::SettingsNetwork::NetworkSettingsModel networkSettings(
      networkClient);
  QString networkClientError;
  if (!networkClient.start(&networkClientError)) {
    qWarning("qindaqt-settings: Network1 client unavailable: %s",
             qPrintable(networkClientError));
  }

  // AGENT-CONTRACT: The Audio route consumes only the public Audio1
  // client/transport boundary. It never links the resident service or the
  // confined WirePlumber/GLib worker and cannot see PipeWire objects.
  QindaQt::Audio::QtAudioTransport audioTransport(
      QDBusConnection::sessionBus());
  QindaQt::Audio::AudioClient audioClient(&audioTransport);
  QindaQt::Apps::SettingsAudio::AudioSettingsModel audioSettings(audioClient);
  audioClient.start();

  // AGENT-CONTRACT: Bluetooth Settings receives only the public Bluetooth1
  // client. Pairing, trust, remove, Agent1, and BlueZ objects remain outside
  // this process boundary; the route model owns its caller-scoped lease.
  QindaQt::Bluetooth::QtBluetoothTransport bluetoothTransport(
      QDBusConnection::sessionBus());
  QindaQt::Bluetooth::BluetoothClient bluetoothClient(&bluetoothTransport);
  QindaQt::Apps::SettingsBluetooth::BluetoothSettingsModel bluetoothSettings(
      bluetoothClient);
  bluetoothClient.start();

  AccessibilityServices accessibility(QDBusConnection::sessionBus());

  // AGENT-CONTRACT: Initialize the Settings navigation controller with the
  // requested route.
  QindaQt::Apps::SettingsCenter::SettingsNavigationController navigation(
      registry, page, arguments.destination, arguments.selection);

  engine.setInitialProperties({
      {QStringLiteral("navigation"),
       QVariant::fromValue(static_cast<QObject *>(&navigation))},
      {QStringLiteral("quietingSettings"),
       QVariant::fromValue(static_cast<QObject *>(&quieting.controller))},
      {QStringLiteral("quietingSchedule"),
       QVariant::fromValue(static_cast<QObject *>(&quieting.schedule))},
      {QStringLiteral("appearanceSettings"),
       QVariant::fromValue(static_cast<QObject *>(&appearanceSettings))},
      {QStringLiteral("windowDecorationSettings"),
       QVariant::fromValue<QObject *>(windowDecorationSettings.get())},
      {QStringLiteral("displaySettings"),
       QVariant::fromValue(static_cast<QObject *>(&displaySettings))},
      {QStringLiteral("networkSettings"),
       QVariant::fromValue(static_cast<QObject *>(&networkSettings))},
      {QStringLiteral("audioSettings"),
       QVariant::fromValue(static_cast<QObject *>(&audioSettings))},
      {QStringLiteral("bluetoothSettings"),
       QVariant::fromValue(static_cast<QObject *>(&bluetoothSettings))},
      {QStringLiteral("accessibilitySettings"),
       QVariant::fromValue(static_cast<QObject *>(&accessibility.model))},
  });

  engine.loadFromModule(QStringLiteral("QindaQt.SettingsApp"),
                        QStringLiteral("Main"));
  if (engine.rootObjects().isEmpty()) {
    return 3;
  }

  return application.exec();
}
