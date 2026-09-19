// SPDX-License-Identifier: GPL-3.0-or-later
#include "launchershortcut.h"

#include "globalshortcutregistrar.h"

#include <QAction>

namespace QindaQt::Shell {

class LauncherShortcutProducer::Private final {
public:
    QAction *action = nullptr;
    bool requestAccepted = false;
    bool activeBinding = false;
};

LauncherShortcutProducer::LauncherShortcutProducer(GlobalShortcutRegistrar &registrar,
                                                   QObject *parent)
    : QObject(parent), m_private(new Private)
{
    m_private->action = new QAction(this);
    m_private->action->setObjectName(stableActionId());
    m_private->action->setText(QStringLiteral("Open the QindaQt application launcher"));
    connect(m_private->action, &QAction::triggered, this,
            &LauncherShortcutProducer::openRequested);
    const auto registration = registrar.registerShortcut(
        *m_private->action, defaultShortcut(), *this,
        [this](bool present) {
            if (m_private->activeBinding == present) {
                return;
            }
            m_private->activeBinding = present;
            Q_EMIT activeBindingPresentChanged();
        });
    m_private->requestAccepted = registration.requestAccepted;
    m_private->activeBinding = registration.activeBindingPresent;
}

LauncherShortcutProducer::~LauncherShortcutProducer() { delete m_private; }

QString LauncherShortcutProducer::stableActionId()
{
    return QStringLiteral("qindaqt_open_launcher");
}

QKeySequence LauncherShortcutProducer::defaultShortcut()
{
    // A key sequence as well as the Meta key: KGlobalAccel needs one to hold
    // the action, and a user whose kwinrc has its own [ModifierOnlyShortcuts]
    // Meta entry keeps a way to reach the launcher.
    return QKeySequence(Qt::ALT | Qt::Key_F1);
}

bool LauncherShortcutProducer::registrationRequestAccepted() const noexcept
{
    return m_private->requestAccepted;
}

bool LauncherShortcutProducer::activeBindingPresent() const noexcept
{
    return m_private->activeBinding;
}

} // namespace QindaQt::Shell
