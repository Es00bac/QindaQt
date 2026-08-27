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

bool NotificationCenterAppletAccess::privatePresentationAllowed() const noexcept
{
    return m_privatePresentationAllowed;
}

void NotificationCenterAppletAccess::toggle()
{
    // AGENT-GUARD: both the panel applet and global shortcut terminate here.
    // Keep the authority check in the C++ facade; a QML enabled state alone is
    // not a security boundary and QAction can be triggered programmatically.
    if (!m_privatePresentationAllowed) {
        return;
    }
    Q_EMIT toggleRequested();
}

void NotificationCenterAppletAccess::publishCenterOpen(bool open)
{
    const bool effectiveOpen = m_privatePresentationAllowed && open;
    if (m_centerOpen == effectiveOpen) {
        return;
    }
    m_centerOpen = effectiveOpen;
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

void NotificationCenterAppletAccess::publishPrivatePresentationAllowed(bool allowed)
{
    if (m_privatePresentationAllowed == allowed) {
        return;
    }
    m_privatePresentationAllowed = allowed;
    if (!allowed) {
        publishCenterOpen(false);
    }
    Q_EMIT privatePresentationAllowedChanged();
}

} // namespace QindaQt::Shell
