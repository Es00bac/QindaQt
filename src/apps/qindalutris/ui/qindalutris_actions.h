// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/app_shell/action_registry.h>

namespace QindaQt::QindaLutris {

// The application's menus as stable command identities. One catalog serves
// both the desktop's global menu and the in-window bar (ADR-0027), so the
// two can never offer different commands.
[[nodiscard]] QList<AppShell::ActionSpec> qindaLutrisActions();

} // namespace QindaQt::QindaLutris
