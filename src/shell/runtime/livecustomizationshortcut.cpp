// SPDX-License-Identifier: GPL-3.0-or-later
#include "livecustomizationshortcut.h"

#include "globalshortcutregistrar.h"

#include <QAction>

#include <utility>

namespace QindaQt::Shell {

LiveCustomizationShortcut::LiveCustomizationShortcut(GlobalShortcutRegistrar &registrar,
                                                     std::function<void()> toggle,
                                                     QObject *parent)
    : QObject(parent)
    , m_action(new QAction(this))
{
    m_action->setObjectName(stableActionId());
    m_action->setText(QStringLiteral("Toggle QindaQt panel edit mode"));
    connect(m_action, &QAction::triggered, this, [callback = std::move(toggle)] {
        if (callback) {
            callback();
        }
    });
    const auto registration = registrar.registerShortcut(
        *m_action, defaultShortcut(), *this, [this](bool present) {
            if (m_activeBindingPresent == present) {
                return;
            }
            m_activeBindingPresent = present;
            Q_EMIT activeBindingPresentChanged();
        });
    m_registrationRequestAccepted = registration.requestAccepted;
    m_activeBindingPresent = registration.activeBindingPresent;
}

QString LiveCustomizationShortcut::stableActionId()
{
    return QStringLiteral("qindaqt_toggle_panel_edit_mode");
}

QKeySequence LiveCustomizationShortcut::defaultShortcut()
{
    return QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_E);
}

} // namespace QindaQt::Shell
