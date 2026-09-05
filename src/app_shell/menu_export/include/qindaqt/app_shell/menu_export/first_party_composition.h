// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/app_shell/menu_export/application_menu_export.h>

#include <QDBusConnection>

#include <memory>

class QObject;
class QWindow;

namespace QindaQt::AppShell {
class ApplicationCoordinator;
}

namespace QindaQt::AppShell::MenuExport {

// AGENT-CONTRACT: The one composition entry first-party executables call after
// their primary window exists (see docs/wiki/shell/global-menu.md, "First-party
// AppShell export"). The caller owns the returned object for the window
// lifetime and must destroy it before the window. A null return is the
// fail-closed outcome: the ordinary in-window menu stays the only authority.
// The bus is injected, never looked up here; the application resolves its own
// session-bus connection.
[[nodiscard]] std::unique_ptr<QObject> composeFirstPartyMenuExport(
    ApplicationCoordinator &coordinator, QWindow &window,
    QDBusConnection sessionBus);

} // namespace QindaQt::AppShell::MenuExport
