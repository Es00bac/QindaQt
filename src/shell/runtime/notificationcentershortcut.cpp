// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationcentershortcut.h"

#include "globalshortcutregistrar.h"

#include <QAction>

#include <utility>

namespace QindaQt::Shell {

NotificationCenterShortcut::NotificationCenterShortcut(
    GlobalShortcutRegistrar &registrar, std::function<void()> toggle,
    QObject *parent)
    : QObject(parent)
    , m_action(new QAction(this))
{
    m_action->setObjectName(stableActionId());
    m_action->setText(QStringLiteral("Toggle QindaQt notification center"));
    connect(m_action, &QAction::triggered, this,
            [callback = std::move(toggle)] {
                if (callback) {
                    callback();
                }
            });
    const auto registration = registrar.registerShortcut(
        *m_action, defaultShortcut(), *this,
        [this](bool present) { setActiveBindingPresent(present); });
    m_registrationRequestAccepted = registration.requestAccepted;
    m_activeBindingPresent = registration.activeBindingPresent;
}

QString NotificationCenterShortcut::stableActionId()
{
    return QStringLiteral("qindaqt_toggle_notification_center");
}

QKeySequence NotificationCenterShortcut::defaultShortcut()
{
    return QKeySequence(Qt::META | Qt::Key_N);
}

QAction *NotificationCenterShortcut::action() const noexcept
{
    return m_action;
}

bool NotificationCenterShortcut::registrationRequestAccepted() const noexcept
{
    return m_registrationRequestAccepted;
}

bool NotificationCenterShortcut::activeBindingPresent() const noexcept
{
    return m_activeBindingPresent;
}

void NotificationCenterShortcut::setActiveBindingPresent(bool present)
{
    if (m_activeBindingPresent == present) {
        return;
    }
    m_activeBindingPresent = present;
    Q_EMIT activeBindingPresentChanged();
}

} // namespace QindaQt::Shell
