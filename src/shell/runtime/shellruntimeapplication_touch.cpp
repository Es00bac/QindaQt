// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellruntimeapplication.h"

#include "desktopcontrolscomposition.h"
#include "edgegesturesubscriber.h"
#include "gatheroverviewcomposition.h"
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
            // The gather overview is what this gesture means now: iconified
            // chips in a lane, rolled-up container cards beside them, and the
            // remaining windows in a scrollable grid. It also replaces KWin's
            // upper-left corner, so the swipe and the corner agree.
            if (m_gatherOverview) {
                m_gatherOverview->toggle();
                return;
            }
            // No gather overview in this session (task-list grants denied):
            // fall back to the command search the gesture used to raise, so
            // the swipe still does something rather than nothing.
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
