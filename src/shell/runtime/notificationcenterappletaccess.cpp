// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationcenterappletaccess.h"

namespace QindaQt::Shell {

NotificationCenterAppletAccess::NotificationCenterAppletAccess(QObject *parent)
    : QObject(parent)
{
}

bool NotificationCenterAppletAccess::centerOpen() const noexcept
{
    return m_centerOpen;
}

void NotificationCenterAppletAccess::toggle()
{
    Q_EMIT toggleRequested();
}

void NotificationCenterAppletAccess::publishCenterOpen(bool open)
{
    if (m_centerOpen == open) {
        return;
    }
    m_centerOpen = open;
    Q_EMIT centerOpenChanged();
}

} // namespace QindaQt::Shell
