// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwintransientmanager.h"

#include "managedwindowregistry.h"

#include <window.h>
#include <workspace.h>

#include <QScopeGuard>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

bool usableTransient(const KWin::Window *window)
{
    return window && !window->isDeleted() && !window->isInternal()
        && !window->isPopupWindow() && (window->isTransient() || window->isDialog());
}

} // namespace

KWinTransientManager::KWinTransientManager(ManagedWindowRegistry &registry,
                                           QObject *parent)
    : QObject(parent)
    , m_registry(registry)
{
    auto *const workspace = KWin::workspace();
    connect(workspace, &KWin::Workspace::windowAdded, this,
            [this](KWin::Window *) {
                QString error;
                if (!synchronizeCurrent(&error) && !error.isEmpty()) {
                    qWarning("QindaQt transient map update failed: %s", qPrintable(error));
                }
            });
    connect(workspace, &KWin::Workspace::windowRemoved, this,
            [this](KWin::Window *) {
                QString error;
                if (!synchronizeCurrent(&error) && !error.isEmpty()) {
                    qWarning("QindaQt transient removal failed: %s", qPrintable(error));
                }
            });
}

KWinTransientManager::~KWinTransientManager() = default;

bool KWinTransientManager::synchronize(const Hybrid::WindowTopology &topology,
                                       QString *error)
{
    QHash<QString, QString> groupOwners;
    for (const auto &containerId : topology.containerIds()) {
        for (const auto &windowId : topology.windowIds(containerId)) {
            groupOwners.insert(windowId, containerId);
        }
    }
    return synchronizeWithOwners(std::move(groupOwners), error);
}

bool KWinTransientManager::synchronizeCurrent(QString *error)
{
    return synchronizeWithOwners(m_groupOwners, error);
}

bool KWinTransientManager::synchronizeWithOwners(
    QHash<QString, QString> groupOwners,
    QString *error)
{
    QVector<TransientSnapshot> snapshots;
    QHash<QString, QPointer<KWin::Window>> transients;
    for (auto *window : KWin::workspace()->windows()) {
        if (!usableTransient(window)) {
            continue;
        }
        auto *owner = groupedOwner(window, groupOwners);
        if (!owner) {
            continue;
        }
        const auto transientId = m_registry.windowId(window);
        const auto ownerId = m_registry.windowId(owner);
        if (transientId.isEmpty() || ownerId.isEmpty()) {
            continue;
        }
        snapshots.append({transientId,
                          ownerId,
                          groupOwners.value(ownerId),
                          window->frameGeometry(),
                          owner->frameGeometry()});
        transients.insert(transientId, window);
    }
    const QSet<QString> groupedIds(groupOwners.keyBegin(), groupOwners.keyEnd());
    if (!m_policy.synchronize(std::move(snapshots), groupedIds, error)) {
        return false;
    }
    // AGENT-GUARD: Pure associations and adapter lookup maps publish as one
    // snapshot. Clearing either live map before validation pairs stale policy
    // state with unrelated KWin pointers after a rejected workspace scan.
    m_groupOwners = std::move(groupOwners);
    m_transients = std::move(transients);
    reconnect();
    for (const auto &association : m_policy.associations()) {
        followContext(association);
    }
    return true;
}

KWin::Window *KWinTransientManager::groupedOwner(
    KWin::Window *transient,
    const QHash<QString, QString> &groupOwners) const
{
    QSet<const KWin::Window *> visited;
    auto *candidate = transient ? transient->transientFor() : nullptr;
    while (candidate && !visited.contains(candidate)) {
        visited.insert(candidate);
        const auto id = m_registry.windowId(candidate);
        if (groupOwners.contains(id)) {
            return candidate;
        }
        candidate = candidate->transientFor();
    }
    if (!transient) {
        return nullptr;
    }
    for (auto *main : transient->allMainWindows()) {
        if (groupOwners.contains(m_registry.windowId(main))) {
            return main;
        }
    }
    return nullptr;
}

void KWinTransientManager::reconnect()
{
    for (const auto &connections : std::as_const(m_connections)) {
        for (const auto &connection : connections) {
            disconnect(connection);
        }
    }
    m_connections.clear();

    QSet<QString> connectedOwners;
    for (const auto &association : m_policy.associations()) {
        auto *transient = m_transients.value(association.transientId).data();
        auto *owner = m_registry.window(association.ownerWindowId);
        if (!transient || !owner) {
            continue;
        }
        auto &transientConnections = m_connections[association.transientId];
        transientConnections.append(connect(
            transient, &KWin::Window::frameGeometryChanged, this,
            [this, id = association.transientId](const KWin::RectF &frame) {
                if (m_applyingGeometry) {
                    return;
                }
                QString error;
                if (!m_policy.transientFrameChanged(id, frame, &error)) {
                    qWarning("QindaQt transient placement update failed: %s",
                             qPrintable(error));
                }
            }));
        transientConnections.append(connect(
            transient, &KWin::Window::transientChanged, this, [this] {
                QString error;
                (void)synchronizeCurrent(&error);
            }));

        if (connectedOwners.contains(association.ownerWindowId)) {
            continue;
        }
        connectedOwners.insert(association.ownerWindowId);
        auto &ownerConnections = m_connections[association.ownerWindowId];
        ownerConnections.append(connect(
            owner, &KWin::Window::frameGeometryChanged, this,
            [this, id = association.ownerWindowId](const KWin::RectF &frame) {
                applyPlacements(m_policy.ownerFrameChanged(id, frame));
            }));
        const auto contextChanged = [this, id = association.ownerWindowId] {
            for (const auto &candidate : m_policy.associations()) {
                if (candidate.ownerWindowId == id) {
                    followContext(candidate);
                }
            }
        };
        ownerConnections.append(connect(owner, &KWin::Window::outputChanged,
                                        this, contextChanged));
        ownerConnections.append(connect(owner, &KWin::Window::desktopsChanged,
                                        this, contextChanged));
        ownerConnections.append(connect(owner, &KWin::Window::activitiesChanged,
                                        this, contextChanged));
    }
}

void KWinTransientManager::followContext(const TransientAssociation &association)
{
    auto *transient = m_transients.value(association.transientId).data();
    auto *owner = m_registry.window(association.ownerWindowId);
    if (!transient || !owner) {
        return;
    }
    if (owner->output() && transient->output() != owner->output()) {
        transient->sendToOutput(owner->output());
    }
    if (transient->desktops() != owner->desktops()) {
        transient->setDesktops(owner->desktops());
    }
    if (transient->activities() != owner->activities()) {
        transient->setOnActivities(owner->activities());
    }
    // AGENT-GUARD: Context following must never raise here. KWin's default
    // raiseWindow(transient) first raises transientFor() ancestors; for a
    // custom A+B group that splits the member block and pulls it above an
    // unrelated active C. KWinHybridGroupStacking owns all group/transient
    // elevation and preserves the outside stack rank as one atomic policy.
}

void KWinTransientManager::applyPlacements(
    const QVector<TransientPlacement> &placements)
{
    m_applyingGeometry = true;
    const auto guard = qScopeGuard([this] { m_applyingGeometry = false; });
    for (const auto &placement : placements) {
        auto *transient = m_transients.value(placement.transientId).data();
        if (!transient) {
            continue;
        }
        transient->moveResize(placement.frame);
        if (const auto association = m_policy.association(placement.transientId)) {
            followContext(*association);
        }
    }
}

std::optional<TransientAssociation> KWinTransientManager::association(
    const QString &transientId) const
{
    return m_policy.association(transientId);
}

} // namespace QindaQt::Compositor::KWinIntegration
