// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/app_shell/app_shell_types.h"

#include <QList>

#include <memory>

class QObject;

namespace QindaQt::AppShell {
class ApplicationCoordinator;
}

namespace QindaQt::Apps::FileManager {

// Stable window-local action identities. AppShell projects them into both the
// visible menu and shortcut dispatch; QML routes every activation to the same
// dialog/controller path used by context-menu actions.
[[nodiscard]] QList<QindaQt::AppShell::ActionSpec> fileManagerActionCatalog();

// Application composition boundary: returns a retained opt-in exporter when
// the QML root is a window. A missing session bus or registrar leaves the
// ordinary in-window menu authoritative and visible.
[[nodiscard]] std::unique_ptr<QObject> composeFileManagerMenuExport(
    QindaQt::AppShell::ApplicationCoordinator &coordinator, QObject *qmlRoot);

} // namespace QindaQt::Apps::FileManager
