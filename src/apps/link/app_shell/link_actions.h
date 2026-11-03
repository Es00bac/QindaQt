// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/app_shell/action_registry.h>

namespace QindaQt::LinkApp {

// The app's menus as stable command identities; one catalog serves the desktop's global menu and
// the in-window bar. Page ids follow app_options.h pageIds().
[[nodiscard]] QList<AppShell::ActionSpec> linkActions();
// "go.messages" -> "messages"; empty when the id is not a page action.
[[nodiscard]] QString pageForActionId(const QString &actionId);

} // namespace QindaQt::LinkApp
