// SPDX-License-Identifier: GPL-3.0-or-later
#include "finder_integration.h"

#include "../app_shell/file_manager_dock_actions.h"

#include <qindaqt/services/settings_client/qt_settings_transport.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QVariant>
#include <QWindow>

namespace QindaQt::Apps::FileManager {

void registerFinderOptions(QCommandLineParser &parser) {
  parser.addOption({QStringLiteral("select"),
                    QStringLiteral("Select this entry of the folder; repeatable (ADR-0273)"),
                    QStringLiteral("name")});
  parser.addOption({QStringLiteral("action"),
                    QStringLiteral("Then run Get Info, Open With, New File or Quick Look (ADR-0273)"),
                    QStringLiteral("id")});
  parser.addOption(
      {QStringLiteral("service"),
       QStringLiteral("Start hidden to serve org.freedesktop.FileManager1 (ADR-0273)")});
}

FinderIntegration composeFinderIntegration(const QCommandLineParser &parser, QObject *qmlRoot,
                                           NavigationController &navigation,
                                           AppShell::ApplicationCoordinator &coordinator,
                                           const QString &startPath, bool chooserMode) {
  FinderIntegration integration;
  auto *window = qobject_cast<QWindow *>(qmlRoot);
  if (window == nullptr) {
    return integration;
  }
  const QDBusConnection bus = QDBusConnection::sessionBus();
  integration.dockPins = std::make_unique<ApplicationDockPins>(
      std::make_unique<Services::SettingsClient::QtSettingsTransport>(bus));
  bindFileManagerDockActions(coordinator, *integration.dockPins);
  window->setProperty("dockPins",
                      QVariant::fromValue(static_cast<QObject *>(integration.dockPins.get())));
  if (chooserMode) {
    return integration;
  }
  integration.windows = std::make_unique<ProcessRevealWindows>(
      *window, navigation, QCoreApplication::applicationFilePath());
  // A name that cannot be an entry would never match one, and only the
  // documented actions may run: anything else is dropped here.
  QStringList names = parser.values(QStringLiteral("select"));
  names.removeIf([](const QString &name) { return !isRevealableName(name); });
  QString action = parser.value(QStringLiteral("action"));
  if (!isRevealAction(action)) {
    action.clear();
  }
  if (!names.isEmpty() || !action.isEmpty()) {
    const bool shown = integration.windows->showHere({startPath, names, action}, {});
    Q_UNUSED(shown);
  }
  integration.service = std::make_unique<FileManager1Service>(*integration.windows);
  if (!integration.service->publish(bus) && !window->isVisible()) {
    window->show();
  }
  return integration;
}

} // namespace QindaQt::Apps::FileManager
