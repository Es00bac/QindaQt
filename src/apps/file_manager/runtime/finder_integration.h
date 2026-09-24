// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "application_dock_pins.h"
#include "file_manager1_service.h"
#include "process_reveal_windows.h"

#include <QString>

#include <memory>

class QCommandLineParser;
class QObject;

namespace QindaQt::AppShell {
class ApplicationCoordinator;
}

namespace QindaQt::Apps::FileManager {

class NavigationController;

// The ADR-0273 command-line options: --select=<name> (repeatable) and
// --action=<id>, the reveal command line FileBoundary::revealArguments()
// writes, and --service, a hidden start for org.freedesktop.FileManager1
// activation.
void registerFinderOptions(QCommandLineParser &parser);

// What one File Manager process keeps alive for the Finder integration
// (ADR-0273). Members are declared in the order they borrow each other.
struct FinderIntegration final {
  std::unique_ptr<ApplicationDockPins> dockPins;
  std::unique_ptr<ProcessRevealWindows> windows;
  std::unique_ptr<FileManager1Service> service;
};

// Composes the window's Finder integration once its QML root has loaded:
// Keep in Dock over Settings1 (the root's `dockPins` and the
// application.keep-in-dock action), the --select/--action reveal of
// `startPath`, and org.freedesktop.FileManager1 on the session bus. A
// workspace picker (`chooserMode`) serves no FileManager1 and reveals
// nothing. Without a usable bus the window works as before, and a --service
// start shows its window instead of staying hidden with nothing to serve.
// AGENT-GUARD: never call this in a --check-* probe mode; the probes stay off
// the session bus. The result must outlive the event loop, and the QML root
// must be destroyed before it.
[[nodiscard]] FinderIntegration composeFinderIntegration(
    const QCommandLineParser &parser, QObject *qmlRoot, NavigationController &navigation,
    AppShell::ApplicationCoordinator &coordinator, const QString &startPath, bool chooserMode);

} // namespace QindaQt::Apps::FileManager
