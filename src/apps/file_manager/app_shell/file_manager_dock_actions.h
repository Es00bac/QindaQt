// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace QindaQt::AppShell {
class ApplicationCoordinator;
}

namespace QindaQt::Apps::FileManager {

class ApplicationDockPins;

// ADR-0273: application.keep-in-dock (Keep in Dock) is enabled while
// `dockPins` can act on the one selected application and checked while that
// application is in the confirmed dock. ui/ApplicationsPlaceActions.qml runs
// the action. Both objects are borrowed; the binding ends with either.
void bindFileManagerDockActions(AppShell::ApplicationCoordinator &coordinator,
                                ApplicationDockPins &dockPins);

} // namespace QindaQt::Apps::FileManager
