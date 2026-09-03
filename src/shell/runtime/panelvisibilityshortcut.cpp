// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelvisibilityshortcut.h"

#include "globalshortcutregistrar.h"
#include "qindaqt/shell_orchestration/panel_interaction_store.h"

#include <QAction>

#include <utility>
#include <vector>

namespace QindaQt::Shell {

class PanelVisibilityShortcutProducer::Private final {
public:
    using Lease = ShellOrchestration::PanelInteractionLease;

    ShellOrchestration::PanelInteractionStore &interactions;
    PanelVisibilityTimerPort &timer;
    QAction *action = nullptr;
    QVector<ShellVisibility::PanelSurfaceIdentity> identities;
    std::vector<Lease> leases;
    quint64 releaseTimer = 0;
    int holdMilliseconds = 1'500;
    bool requestAccepted = false;
    bool activeBinding = false;
};

PanelVisibilityShortcutProducer::PanelVisibilityShortcutProducer(
    GlobalShortcutRegistrar &registrar,
    ShellOrchestration::PanelInteractionStore &interactions,
    PanelVisibilityTimerPort &timer, QObject *parent)
    : QObject(parent)
    , m_private(new Private{interactions, timer, nullptr, {}, {}, 0, 1'500,
                            false, false})
{
    m_private->action = new QAction(this);
    m_private->action->setObjectName(stableActionId());
    m_private->action->setText(QStringLiteral("Reveal QindaQt panels"));
    connect(m_private->action, &QAction::triggered, this,
            &PanelVisibilityShortcutProducer::triggerReveal);
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

PanelVisibilityShortcutProducer::~PanelVisibilityShortcutProducer()
{
    m_private->timer.cancel(m_private->releaseTimer);
    delete m_private;
}

QString PanelVisibilityShortcutProducer::stableActionId()
{
    return QStringLiteral("qindaqt_reveal_panels");
}

QKeySequence PanelVisibilityShortcutProducer::defaultShortcut()
{
    return QKeySequence(Qt::META | Qt::Key_Space);
}

QAction *PanelVisibilityShortcutProducer::action() const noexcept
{
    return m_private->action;
}

bool PanelVisibilityShortcutProducer::registrationRequestAccepted() const noexcept
{
    return m_private->requestAccepted;
}

bool PanelVisibilityShortcutProducer::activeBindingPresent() const noexcept
{
    return m_private->activeBinding;
}

void PanelVisibilityShortcutProducer::setIdentities(
    QVector<ShellVisibility::PanelSurfaceIdentity> identities)
{
    if (m_private->identities == identities) {
        return;
    }
    m_private->timer.cancel(m_private->releaseTimer);
    m_private->releaseTimer = 0;
    m_private->leases.clear();
    m_private->identities = std::move(identities);
}

void PanelVisibilityShortcutProducer::setHoldMilliseconds(int holdMilliseconds)
{
    if (holdMilliseconds >= 1 && holdMilliseconds <= 10'000) {
        m_private->holdMilliseconds = holdMilliseconds;
    }
}

void PanelVisibilityShortcutProducer::triggerReveal()
{
    m_private->timer.cancel(m_private->releaseTimer);
    m_private->releaseTimer = 0;
    m_private->leases.clear();
    for (const auto &identity : std::as_const(m_private->identities)) {
        QString error;
        auto lease = m_private->interactions.acquire(
            identity, ShellOrchestration::PanelInteractionKind::Reveal, &error);
        if (lease) {
            m_private->leases.push_back(std::move(*lease));
        }
    }
    if (m_private->leases.empty()) {
        return;
    }
    m_private->releaseTimer = m_private->timer.schedule(
        m_private->holdMilliseconds, [this] {
            m_private->releaseTimer = 0;
            m_private->leases.clear();
        });
}

} // namespace QindaQt::Shell
