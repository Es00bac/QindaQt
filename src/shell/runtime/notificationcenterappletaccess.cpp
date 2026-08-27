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

bool NotificationCenterAppletAccess::doNotDisturbEnabled() const noexcept
{
    return m_doNotDisturbEnabled;
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

void NotificationCenterAppletAccess::publishDoNotDisturbEnabled(bool enabled)
{
    if (m_doNotDisturbEnabled == enabled) {
        return;
    }
    m_doNotDisturbEnabled = enabled;
    Q_EMIT doNotDisturbEnabledChanged();
}

} // namespace QindaQt::Shell
