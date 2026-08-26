// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinchromemanager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

struct PreparedEntry final
{
    HybridChrome::ChromeRenderPlan plan;
    QMap<QString, QString> tabRepresentatives;
};

using PreparedEntries = std::map<QString, PreparedEntry>;

bool reject(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

void collectNodeIds(const Core::LayoutNode &node,
                    QStringList *windows,
                    QStringList *dividers)
{
    if (node.isLeaf()) {
        windows->append(node.windowId());
        return;
    }
    dividers->append(node.id());
    // Valid topology splits always have both children. Keep this helper total
    // so a malformed external snapshot is rejected instead of dereferenced.
    if (node.firstChild()) {
        collectNodeIds(*node.firstChild(), windows, dividers);
    }
    if (node.secondChild()) {
        collectNodeIds(*node.secondChild(), windows, dividers);
    }
}

QByteArray topologyShape(const Hybrid::WindowTopology &topology)
{
    QJsonObject shape;
    shape.insert(QStringLiteral("independentWindowIds"),
                 QJsonArray::fromStringList(topology.independentWindowIds()));
    QJsonArray containers;
    for (const auto &containerId : topology.containerIds()) {
        const auto *container = topology.container(containerId);
        if (container) {
            containers.append(container->toJson());
        }
    }
    shape.insert(QStringLiteral("containers"), containers);
    return QJsonDocument(shape).toJson(QJsonDocument::Compact);
}

bool sameIds(const QStringList &expected, const QStringList &actual)
{
    auto sortedExpected = expected;
    auto sortedActual = actual;
    std::sort(sortedExpected.begin(), sortedExpected.end());
    std::sort(sortedActual.begin(), sortedActual.end());
    return sortedExpected == sortedActual
        && std::adjacent_find(sortedActual.cbegin(), sortedActual.cend()) == sortedActual.cend();
}

bool preparePlan(const Core::WindowContainer &container,
                 HybridChrome::ChromeRenderPlan plan,
                 PreparedEntry *prepared,
                 QString *error)
{
    if (plan.containerId != container.id()) {
        return reject(error, QStringLiteral("chrome plan key '%1' identifies container '%2'")
                                 .arg(container.id(), plan.containerId));
    }
    if (!plan.outerFrame.isValid() || !std::isfinite(plan.outerFrame.x())
        || !std::isfinite(plan.outerFrame.y()) || !std::isfinite(plan.outerFrame.width())
        || !std::isfinite(plan.outerFrame.height())) {
        return reject(error, QStringLiteral("container '%1' has invalid chrome geometry")
                                 .arg(container.id()));
    }

    QStringList expectedTabs;
    QStringList actualTabs;
    QMap<QString, QString> representatives;
    qsizetype activeTabs = 0;
    for (const auto &page : container.pages()) {
        expectedTabs.append(page.id());
        QStringList windows;
        QStringList dividers;
        collectNodeIds(page.root(), &windows, &dividers);
        if (!windows.isEmpty()) {
            representatives.insert(page.id(), windows.constFirst());
        }
    }
    for (const auto &tab : plan.tabs) {
        actualTabs.append(tab.tabId);
        activeTabs += tab.active ? 1 : 0;
    }
    if (expectedTabs != actualTabs) {
        return reject(error, QStringLiteral("container '%1' chrome tabs do not preserve topology page order")
                                 .arg(container.id()));
    }
    if ((!expectedTabs.isEmpty() && activeTabs != 1)
        || (expectedTabs.isEmpty() && activeTabs != 0)) {
        return reject(error, QStringLiteral("container '%1' must expose exactly its active topology page")
                                 .arg(container.id()));
    }
    for (const auto &tab : plan.tabs) {
        if (tab.active != (tab.tabId == container.activePageId())) {
            return reject(error, QStringLiteral("container '%1' chrome active tab disagrees with topology")
                                     .arg(container.id()));
        }
    }

    QStringList expectedMembers;
    QStringList expectedDividers;
    if (const auto *activePage = container.page(container.activePageId())) {
        collectNodeIds(activePage->root(), &expectedMembers, &expectedDividers);
    }
    QStringList actualMembers;
    for (const auto &member : plan.members) {
        actualMembers.append(member.memberId);
    }
    QStringList actualDividers;
    for (const auto &divider : plan.dividers) {
        actualDividers.append(divider.dividerId);
    }
    if (!sameIds(expectedMembers, actualMembers)) {
        return reject(error, QStringLiteral("container '%1' chrome members disagree with its active page")
                                 .arg(container.id()));
    }
    if (!sameIds(expectedDividers, actualDividers)) {
        return reject(error, QStringLiteral("container '%1' chrome dividers disagree with its active page")
                                 .arg(container.id()));
    }

    prepared->plan = std::move(plan);
    prepared->tabRepresentatives = std::move(representatives);
    return true;
}

bool prepareSnapshot(const Hybrid::WindowTopology &snapshot,
                     const KWinChromeManager::ChromePlanMap &plans,
                     QStringList *stackingOrder,
                     PreparedEntries *prepared,
                     QString *error)
{
    const auto validation = snapshot.validate();
    if (!validation.valid) {
        return reject(error, QStringLiteral("invalid topology snapshot: %1").arg(validation.message));
    }
    const auto containerIds = snapshot.containerIds();
    if (!sameIds(containerIds, plans.keys())) {
        return reject(error, QStringLiteral("chrome plans must name every topology container exactly once"));
    }
    if (stackingOrder->isEmpty()) {
        *stackingOrder = containerIds;
    } else if (!sameIds(containerIds, *stackingOrder)) {
        return reject(error, QStringLiteral("chrome stacking order must name every container exactly once"));
    }
    for (const auto &containerId : containerIds) {
        const auto *container = snapshot.container(containerId);
        PreparedEntry entry;
        if (!container || !preparePlan(*container, plans.value(containerId), &entry, error)) {
            return false;
        }
        prepared->emplace(containerId, std::move(entry));
    }
    return true;
}

} // namespace

KWinChromeManager::KWinChromeManager(ChromeOverlayFactory &factory, QObject *parent)
    : QObject(parent)
    , m_factory(&factory)
{
    qRegisterMetaType<HybridChrome::WindowAction>();
    qRegisterMetaType<HybridChrome::ChromeDragEvent>();
    qRegisterMetaType<ChromeWindowActionRequest>();
}

KWinChromeManager::~KWinChromeManager() { clear(); }

bool KWinChromeManager::updateFromSnapshot(const Hybrid::WindowTopology &snapshot,
                                            const ChromePlanMap &plans,
                                            QStringList stackingOrder,
                                            QString *error)
{
    if (error) {
        error->clear();
    }
    if (QThread::currentThread() != thread()) {
        return reject(error, QStringLiteral("chrome reconciliation must run on its QObject thread"));
    }
    const auto shape = topologyShape(snapshot);
    if (m_hasSnapshot && snapshot.revision() < m_topologyRevision) {
        return reject(error, QStringLiteral("stale topology revision %1 follows %2")
                                 .arg(snapshot.revision()).arg(m_topologyRevision));
    }
    if (m_hasSnapshot && snapshot.revision() == m_topologyRevision
        && shape != m_topologyShape) {
        return reject(error, QStringLiteral("topology changed without advancing its revision"));
    }

    PreparedEntries prepared;
    if (!prepareSnapshot(snapshot, plans, &stackingOrder, &prepared, error)) {
        return false;
    }

    QMap<QString, bool> visibilityBefore;
    for (const auto &[containerId, entry] : m_entries) {
        visibilityBefore.insert(containerId, entry.overlay->isVisible());
    }

    // Stage every fallible creation before touching a published overlay.
    std::map<QString, std::unique_ptr<ChromeOverlay>> created;
    for (const auto &[containerId, ignored] : prepared) {
        Q_UNUSED(ignored)
        if (!m_entries.contains(containerId)) {
            auto overlay = m_factory->create(containerId);
            if (!overlay) {
                return reject(error, QStringLiteral("failed to create chrome overlay for '%1'")
                                         .arg(containerId));
            }
            created.emplace(containerId, std::move(overlay));
        }
    }

    for (auto &[containerId, preparedEntry] : prepared) {
        auto existing = m_entries.find(containerId);
        if (existing != m_entries.end()) {
            existing->second.overlay->setRenderPlan(preparedEntry.plan);
            existing->second.plan = preparedEntry.plan;
            existing->second.tabRepresentatives = preparedEntry.tabRepresentatives;
            continue;
        }
        auto staged = created.find(containerId);
        staged->second->setRenderPlan(preparedEntry.plan);
        m_entries.emplace(containerId,
                          Entry{std::move(staged->second), std::move(preparedEntry.plan),
                                std::move(preparedEntry.tabRepresentatives)});
    }

    QStringList removedIds;
    for (const auto &[containerId, entry] : m_entries) {
        Q_UNUSED(entry)
        if (!prepared.contains(containerId)) {
            removedIds.append(containerId);
        }
    }
    for (const auto &containerId : removedIds) {
        auto found = m_entries.find(containerId);
        auto overlay = std::move(found->second.overlay);
        m_entries.erase(found);
        overlay->closeOverlay();
    }
    for (const auto &containerId : stackingOrder) {
        auto &overlay = m_entries.at(containerId).overlay;
        if (m_contextQuarantine.contains(containerId)) {
            overlay->hideOverlay();
        } else {
            overlay->showOverlay();
        }
    }

    m_stackingOrder = std::move(stackingOrder);
    m_topologyShape = shape;
    m_topologyRevision = snapshot.revision();
    m_hasSnapshot = true;
    // The accepted snapshot is the first trustworthy evidence that a released
    // container is truly absent. Failed/stale updates leave quarantine intact.
    m_contextQuarantine.reconcilePublishedContainers(snapshot.containerIds());
    if (m_pointerHover && !m_entries.contains(m_pointerHover->containerId)) {
        m_pointerHover.reset();
    }
    applyPointerHover();
    // AGENT-GUARD: Publish visibility only after the entire manager snapshot
    // is coherent. Accessibility callbacks query plans synchronously and must
    // never observe a half-removed reconciliation.
    for (auto iterator = visibilityBefore.cbegin();
         iterator != visibilityBefore.cend(); ++iterator) {
        if (!m_entries.contains(iterator.key()) && iterator.value()) {
            Q_EMIT overlayVisibilityChanged(iterator.key(), false);
        }
    }
    for (const auto &[containerId, entry] : m_entries) {
        const bool visible = entry.overlay->isVisible();
        if (visibilityBefore.value(containerId, false) != visible) {
            Q_EMIT overlayVisibilityChanged(containerId, visible);
        }
    }
    return true;
}

void KWinChromeManager::quarantineContainer(const QString &containerId)
{
    m_contextQuarantine.quarantine(containerId);
    setOverlayVisible(containerId, false);
}

void KWinChromeManager::markContainerContextCoherent(
    const QString &containerId)
{
    m_contextQuarantine.markCoherent(containerId);
}

void KWinChromeManager::clear() noexcept
{
    // Publish the empty state before invoking overlay close hooks. This keeps
    // re-entrant observers from finding half-destroyed scene items.
    auto entries = std::move(m_entries);
    m_entries.clear();
    m_stackingOrder.clear();
    m_topologyShape.clear();
    m_pointerHover.reset();
    m_topologyRevision = 0;
    m_hasSnapshot = false;
    // AGENT-GUARD: clear() also serves compositor scene recreation. Retain the
    // context quarantine so fresh ImageItems cannot revive unsafe chrome.
    for (auto &[containerId, entry] : entries) {
        const bool wasVisible = entry.overlay->isVisible();
        entry.overlay->closeOverlay();
        if (wasVisible) {
            Q_EMIT overlayVisibilityChanged(containerId, false);
        }
    }
}

} // namespace QindaQt::Compositor::KWinIntegration
