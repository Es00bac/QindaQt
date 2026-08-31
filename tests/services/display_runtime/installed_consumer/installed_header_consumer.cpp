// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_runtime/resident_display_runtime.h>
#include <qindaqt/services/display_runtime/session_safety_port.h>
#include <qindaqt/services/display_runtime/state_root.h>

#include <memory>
#include <type_traits>

using namespace QindaQt::DisplayRuntime;

static_assert(std::is_same_v<decltype(makeQtSessionSafetyPort(
                                 std::declval<const QDBusConnection &>(),
                                 std::declval<const QDBusConnection &>())),
                             std::unique_ptr<SessionSafetyPort>>);

StateRootSelection installedHeaderSelection()
{
    return selectStateRoot({.explicitPath = QStringLiteral("/state/qindaqt"),
                            .systemdStateDirectory = {},
                            .xdgStateHome = {},
                            .home = {}});
}
