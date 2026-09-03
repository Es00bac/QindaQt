// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/app_shell/app_shell_types.h"

#include <QList>

namespace QindaQt::Apps::FileManager {

// Stable window-local action identities. AppShell projects them into both the
// visible menu and shortcut dispatch; QML routes every activation to the same
// dialog/controller path used by context-menu actions.
[[nodiscard]] QList<QindaQt::AppShell::ActionSpec> fileManagerActionCatalog();

} // namespace QindaQt::Apps::FileManager
