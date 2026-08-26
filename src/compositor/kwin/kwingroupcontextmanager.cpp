// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwingroupcontextmanager.h"

#include "hybridgroupcontext.h"
#include "managedwindowregistry.h"

#include <window.h>

#include <QScopedValueRollback>
#include <QTimer>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {

KWinGroupContextManager::KWinGroupContextManager(
    ManagedWindowRegistry &registry,
    GroupContextApply apply,
    GroupContextEventSuppression eventsSuppressed,
    QObject *parent)
    : QObject(parent)
    , m_registry(registry)
    , m_apply(std::move(apply))
    , m_eventsSuppressed(std::move(eventsSuppressed))
{
}

KWinGroupContextManager::~KWinGroupContextManager()
{
    shutdown();
}

bool KWinGroupContextManager::eventsAreSuppressed() const
{
    return m_shutdown || m_applying
        || (m_eventsSuppressed && m_eventsSuppressed());
}

void KWinGroupContextManager::synchronize(
    const Hybrid::WindowTopology &topology)
{
    if (m_shutdown) {
        return;
    }
    const auto pending = m_pendingSourceByContainer;
    disconnectWindows();
    m_containerForWindow.clear();

    for (const auto &containerId : topology.containerIds()) {
        const auto memberIds = topology.windowIds(containerId);
        for (const auto &windowId : memberIds) {
            auto *window = m_registry.window(windowId);
            if (!window) {
                continue;
            }
            m_containerForWindow.insert(windowId, containerId);
            const auto queue = [this, windowId] {
                queueMemberContext(windowId);
            };
            auto &connections = m_windowConnections[windowId];
            connections.append(connect(window, &KWin::Window::outputChanged,
                                       this, queue));
            connections.append(connect(window, &KWin::Window::desktopsChanged,
                                       this, queue));
            connections.append(connect(window, &KWin::Window::activitiesChanged,
                                       this, queue));
            connections.append(connect(
                window, &KWin::Window::keepAboveChanged, this,
                [queue](bool) { queue(); }));
            connections.append(connect(
                window, &KWin::Window::keepBelowChanged, this,
                [queue](bool) { queue(); }));
        }
    }
    QHash<QString, QString> liveOwners;
    for (auto iterator = m_containerForWindow.cbegin();
         iterator != m_containerForWindow.cend(); ++iterator) {
        liveOwners.insert(iterator.key(), m_registry.owner(iterator.key()));
    }
    // AGENT-GUARD: A layer change can queue the chrome stack scheduler before
    // keepAboveChanged queues this manager. That earlier chrome refresh must
    // reconnect observers without erasing the still-live adoption request.
    m_pendingSourceByContainer = retainValidGroupContextSources(
        pending, m_containerForWindow, liveOwners);
}

void KWinGroupContextManager::queueMemberContext(const QString &windowId)
{
    if (eventsAreSuppressed()) {
        return;
    }
    const auto containerId = m_containerForWindow.value(windowId);
    if (containerId.isEmpty() || m_registry.owner(windowId) != containerId) {
        return;
    }
    // AGENT-GUARD: keepAbove(true) may synchronously clear keepBelow and emit
    // both signals. Applying inline would adopt the intermediate state and
    // recurse through every peer; the last source in this turn is canonical.
    m_pendingSourceByContainer.insert(containerId, windowId);
    if (m_drainScheduled) {
        return;
    }
    m_drainScheduled = true;
    QTimer::singleShot(0, this, [this] { drainPending(); });
}

void KWinGroupContextManager::drainPending()
{
    m_drainScheduled = false;
    if (m_shutdown) {
        m_pendingSourceByContainer.clear();
        return;
    }
    const auto pending = std::exchange(m_pendingSourceByContainer, {});
    QScopedValueRollback<bool> applying(m_applying, true);
    for (auto iterator = pending.cbegin(); iterator != pending.cend(); ++iterator) {
        if (m_eventsSuppressed && m_eventsSuppressed()) {
            continue;
        }
        if (m_containerForWindow.value(iterator.value()) != iterator.key()
            || m_registry.owner(iterator.value()) != iterator.key()) {
            continue;
        }
        if (m_apply) {
            m_apply(iterator.key(), iterator.value());
        }
    }
}

void KWinGroupContextManager::disconnectWindows() noexcept
{
    for (const auto &connections : std::as_const(m_windowConnections)) {
        for (const auto &connection : connections) {
            disconnect(connection);
        }
    }
    m_windowConnections.clear();
}

void KWinGroupContextManager::shutdown() noexcept
{
    if (m_shutdown) {
        return;
    }
    m_shutdown = true;
    disconnectWindows();
    m_containerForWindow.clear();
    m_pendingSourceByContainer.clear();
}

} // namespace QindaQt::Compositor::KWinIntegration
