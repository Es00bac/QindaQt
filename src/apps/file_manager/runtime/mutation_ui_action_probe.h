// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

class QObject;

namespace QindaQt::AppShell {
class ApplicationCoordinator;
}

namespace QindaQt::Apps::FileManager {
class ClipboardController;
class MutationController;
class NavigationController;

// Drives the production QML action/dialog seam against a disposable fixture.
// The caller owns every object and must keep them on the GUI thread for the
// complete synchronous probe. This diagnostic performs no ambient discovery.
[[nodiscard]] bool verifyMutationUiActions(
    QObject *root, QindaQt::AppShell::ApplicationCoordinator *coordinator,
    NavigationController *navigation, MutationController *mutation,
    ClipboardController *clipboard, const QString &fixtureRoot, QString *error);

} // namespace QindaQt::Apps::FileManager
