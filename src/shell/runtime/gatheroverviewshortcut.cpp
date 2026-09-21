// SPDX-License-Identifier: GPL-3.0-or-later
#include "gatheroverviewshortcut.h"

#include "globalshortcutregistrar.h"

#include <QAction>

namespace QindaQt::Shell {

class GatherOverviewShortcutProducer::Private final {
public:
    QAction *action = nullptr;
    bool requestAccepted = false;
    bool activeBinding = false;
};

GatherOverviewShortcutProducer::GatherOverviewShortcutProducer(
    GlobalShortcutRegistrar &registrar, QObject *parent)
    : QObject(parent), m_private(new Private)
{
    m_private->action = new QAction(this);
    m_private->action->setObjectName(stableActionId());
    m_private->action->setText(
        QStringLiteral("Show the QindaQt gather overview"));
    connect(m_private->action, &QAction::triggered, this,
            &GatherOverviewShortcutProducer::toggleRequested);
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

GatherOverviewShortcutProducer::~GatherOverviewShortcutProducer()
{
    delete m_private;
}

QString GatherOverviewShortcutProducer::stableActionId()
{
    // AGENT-GUARD: a machine whose kwinrc already carries a seeded
    // [ModifierOnlyShortcuts] or BorderActivate entry naming this id loses the
    // gesture if the id is renamed. Treat it as published.
    return QStringLiteral("qindaqt_toggle_gather_overview");
}

QKeySequence GatherOverviewShortcutProducer::defaultShortcut()
{
    return QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_G);
}

bool GatherOverviewShortcutProducer::registrationRequestAccepted() const noexcept
{
    return m_private->requestAccepted;
}

bool GatherOverviewShortcutProducer::activeBindingPresent() const noexcept
{
    return m_private->activeBinding;
}

} // namespace QindaQt::Shell
