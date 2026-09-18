// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellruntimeapplication.h"

#include "desktopcontrolscomposition.h"
#include "edgegesturesubscriber.h"
#include "notificationcenterappletaccess.h"
#include "qindaqt/shell/desktop_controls/desktop_controls_access.h"

#include <QDBusConnection>

namespace QindaQt::Shell {

// ADR-0205: touch edge swipes announced by the compositor open the same
// surfaces their buttons and shortcuts do. Kept out of initializeRuntime,
// which is already past the review size.
void ShellRuntimeApplication::installEdgeGestures()
{
    m_edgeGestures = std::make_unique<EdgeGestureSubscriber>(EdgeGestureHandlers{
        .overview = [this] {
            if (m_desktopControls && m_desktopControls->access() != nullptr) {
                m_desktopControls->access()->requestOverview();
            }
        },
        .notifications = [this] {
            if (m_notificationCenterAccess) {
                m_notificationCenterAccess->toggle();
            }
        },
        .taskSwitcher = [] { invokeWalkThroughWindows(QDBusConnection::sessionBus()); }});
}

} // namespace QindaQt::Shell
