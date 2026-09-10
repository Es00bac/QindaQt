// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/app_shell/app_shell_types.h"

#include <QList>

#include <memory>

class QObject;

namespace QindaQt::AppShell {
class ApplicationCoordinator;
}

namespace QindaQt::Apps::Calendar {

// Stable window-local action identities for the Calendar app. AppShell
// projects them into both the visible menu and shortcut dispatch; QML routes
// every activation to the same controller/dialog path used by on-screen
// buttons. View actions are checkable and exclusive by convention: exactly
// one of view.month/view.week/view.day is checked at any time, synced from
// CalendarController::viewMode.
[[nodiscard]] QList<QindaQt::AppShell::ActionSpec> calendarActionCatalog();

// Application composition boundary: returns a retained opt-in exporter when
// the QML root is a window. A missing session bus or registrar leaves the
// ordinary in-window menu authoritative and visible.
[[nodiscard]] std::unique_ptr<QObject> composeCalendarMenuExport(
    QindaQt::AppShell::ApplicationCoordinator &coordinator, QObject *qmlRoot);

} // namespace QindaQt::Apps::Calendar
