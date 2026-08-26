// SPDX-License-Identifier: LGPL-3.0-or-later
#include "windowcontainer.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <utility>

namespace QindaQt::Core {
namespace {

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

} // namespace

ContainerPage::ContainerPage(QString pageId, LayoutNode root)
    : m_id(std::move(pageId))
    , m_root(std::move(root))
{
}

WindowContainer::WindowContainer(QString containerId)
    : m_id(std::move(containerId))
{
}

const ContainerPage *WindowContainer::page(const QString &pageId) const noexcept
{
    const auto match = std::find_if(m_pages.cbegin(), m_pages.cend(), [&](const auto &page) {
        return page.id() == pageId;
    });
    return match == m_pages.cend() ? nullptr : &*match;
}

const LayoutNode *WindowContainer::findNode(const QString &nodeId) const noexcept
{
    for (const auto &candidate : m_pages) {
        if (const auto *match = candidate.root().findNode(nodeId)) {
            return match;
        }
    }
    return nullptr;
}

const LayoutNode *WindowContainer::findWindow(const QString &windowId) const noexcept
{
    for (const auto &candidate : m_pages) {
        if (const auto *match = candidate.root().findWindow(windowId)) {
            return match;
        }
    }
    return nullptr;
}

std::optional<QString> WindowContainer::singleWindowId() const
{
    if (m_pages.size() != 1 || !m_pages.constFirst().root().isLeaf()) {
        return std::nullopt;
    }
    return m_pages.constFirst().root().windowId();
}

bool WindowContainer::containsStructuralId(const QString &id) const noexcept
{
    if (m_id == id || page(id)) {
        return true;
    }
    return findNode(id) != nullptr;
}

bool WindowContainer::addPage(QString pageId,
                              QString leafNodeId,
                              QString windowId,
                              QString *error)
{
    return addPage(ContainerPage(std::move(pageId),
                                 LayoutNode::makeLeaf(std::move(leafNodeId),
                                                      std::move(windowId))),
                   error);
}

bool WindowContainer::addPage(ContainerPage pageToAdd, QString *error)
{
    // AGENT-GUARD: Validate a copy so an invalid imported subtree never leaves
    // the live model partially modified.
    WindowContainer candidate(*this);
    candidate.m_pages.append(std::move(pageToAdd));
    if (candidate.m_pages.size() == 1) {
        candidate.m_activePageId = candidate.m_pages.constFirst().id();
    }

    const auto result = candidate.validate();
    if (!result.valid) {
        return fail(error, result.message);
    }
    *this = std::move(candidate);
    return true;
}

bool WindowContainer::activatePage(const QString &pageId, QString *error)
{
    const auto state = validate();
    if (!state.valid) {
        return fail(error, state.message);
    }
    if (!page(pageId)) {
        return fail(error, QStringLiteral("unknown page ID '%1'").arg(pageId));
    }
    m_activePageId = pageId;
    return true;
}

bool WindowContainer::movePage(const QString &pageId,
                               qsizetype destinationIndex,
                               QString *error)
{
    const auto state = validate();
    if (!state.valid) {
        return fail(error, state.message);
    }
    if (destinationIndex < 0 || destinationIndex >= m_pages.size()) {
        return fail(error,
                    QStringLiteral("destination page index %1 is out of range")
                        .arg(destinationIndex));
    }

    const auto match = std::find_if(m_pages.cbegin(), m_pages.cend(), [&](const auto &page) {
        return page.id() == pageId;
    });
    if (match == m_pages.cend()) {
        return fail(error, QStringLiteral("unknown page ID '%1'").arg(pageId));
    }
    const auto sourceIndex = std::distance(m_pages.cbegin(), match);
    if (sourceIndex == destinationIndex) {
        return fail(error, QStringLiteral("page is already at destination index"));
    }

    // AGENT-NOTE: destinationIndex is the final index, matching QVector::move.
    // The active page is ID-based, so reordering requires no active-state rewrite.
    m_pages.move(sourceIndex, destinationIndex);
    Q_ASSERT(validate().valid);
    return true;
}

bool WindowContainer::splitWindow(const SplitRequest &request, QString *error)
{
    const auto state = validate();
    if (!state.valid) {
        return fail(error, state.message);
    }
    if (!findWindow(request.targetWindowId)) {
        return fail(error,
                    QStringLiteral("unknown target window ID '%1'").arg(request.targetWindowId));
    }
    if (request.newWindowId.isEmpty() || request.newLeafNodeId.isEmpty()
        || request.splitNodeId.isEmpty()) {
        return fail(error, QStringLiteral("new window, leaf, and split IDs must be non-empty"));
    }
    if (findWindow(request.newWindowId)) {
        return fail(error,
                    QStringLiteral("duplicate window ID '%1'").arg(request.newWindowId));
    }
    if (request.newLeafNodeId == request.splitNodeId
        || containsStructuralId(request.newLeafNodeId)
        || containsStructuralId(request.splitNodeId)) {
        return fail(error, QStringLiteral("new leaf and split IDs must be unique"));
    }
    if (!std::isfinite(request.ratio) || request.ratio <= 0.0 || request.ratio >= 1.0) {
        return fail(error, QStringLiteral("split ratio must be between 0 and 1"));
    }

    for (auto &candidate : m_pages) {
        if (candidate.m_root.splitWindow(request.targetWindowId,
                                         LayoutNode::makeLeaf(request.newLeafNodeId,
                                                              request.newWindowId),
                                         request.splitNodeId,
                                         request.orientation,
                                         request.ratio,
                                         request.position)) {
            Q_ASSERT(validate().valid);
            return true;
        }
    }
    return fail(error, QStringLiteral("target window disappeared during split"));
}

bool WindowContainer::swapWindows(const QString &firstWindowId,
                                  const QString &secondWindowId,
                                  QString *error)
{
    const auto state = validate();
    if (!state.valid) {
        return fail(error, state.message);
    }
    if (firstWindowId.isEmpty() || secondWindowId.isEmpty()) {
        return fail(error, QStringLiteral("window IDs must be non-empty"));
    }
    if (firstWindowId == secondWindowId) {
        return fail(error, QStringLiteral("window IDs must identify different windows"));
    }

    LayoutNode *firstLeaf = nullptr;
    LayoutNode *secondLeaf = nullptr;
    for (auto &candidate : m_pages) {
        if (!firstLeaf) {
            firstLeaf = candidate.m_root.findWindowMutable(firstWindowId);
        }
        if (!secondLeaf) {
            secondLeaf = candidate.m_root.findWindowMutable(secondWindowId);
        }
    }
    if (!firstLeaf || !secondLeaf) {
        const auto &missingId = firstLeaf ? secondWindowId : firstWindowId;
        return fail(error, QStringLiteral("unknown window ID '%1'").arg(missingId));
    }

    // AGENT-GUARD: Swap only leaf payloads. Moving/rebuilding nodes would break
    // persisted structural handles used by dividers, focus history, and restore.
    std::swap(firstLeaf->m_windowId, secondLeaf->m_windowId);
    Q_ASSERT(validate().valid);
    return true;
}

bool WindowContainer::setSplitRatio(const QString &splitNodeId,
                                    double ratio,
                                    QString *error)
{
    const auto state = validate();
    if (!state.valid) {
        return fail(error, state.message);
    }
    if (!std::isfinite(ratio) || ratio <= 0.0 || ratio >= 1.0) {
        return fail(error, QStringLiteral("split ratio must be between 0 and 1"));
    }
    const auto *node = findNode(splitNodeId);
    if (!node || !node->isSplit()) {
        return fail(error, QStringLiteral("unknown split node ID '%1'").arg(splitNodeId));
    }

    for (auto &candidate : m_pages) {
        if (candidate.m_root.setSplitRatio(splitNodeId, ratio)) {
            Q_ASSERT(validate().valid);
            return true;
        }
    }
    return fail(error, QStringLiteral("split node disappeared during update"));
}

std::optional<DetachedWindow> WindowContainer::detachWindow(const QString &windowId,
                                                            QString *error)
{
    const auto state = validate();
    if (!state.valid) {
        fail(error, state.message);
        return std::nullopt;
    }
    if (!findWindow(windowId)) {
        fail(error, QStringLiteral("unknown window ID '%1'").arg(windowId));
        return std::nullopt;
    }

    for (qsizetype index = 0; index < m_pages.size(); ++index) {
        auto &candidate = m_pages[index];
        const QString sourcePageId = candidate.id();
        QString leafNodeId;
        const auto outcome = candidate.m_root.removeWindow(windowId, &leafNodeId);
        if (outcome == LayoutNode::RemovalOutcome::NotFound) {
            continue;
        }

        if (outcome == LayoutNode::RemovalOutcome::RemoveThisNode) {
            const bool removedActivePage = sourcePageId == m_activePageId;
            m_pages.removeAt(index);
            if (m_pages.isEmpty()) {
                m_activePageId.clear();
            } else if (removedActivePage) {
                // AGENT-NOTE: Prefer the page that shifted into the removed
                // slot, falling back to the preceding page at the end.
                const auto nextIndex = std::min(index, m_pages.size() - 1);
                m_activePageId = m_pages[nextIndex].id();
            }
        }

        Q_ASSERT(validate().valid);
        return DetachedWindow{windowId, leafNodeId, sourcePageId};
    }

    fail(error, QStringLiteral("window disappeared during detach"));
    return std::nullopt;
}

bool WindowContainer::removeWindow(const QString &windowId, QString *error)
{
    return detachWindow(windowId, error).has_value();
}

ValidationResult WindowContainer::validate() const
{
    if (m_id.isEmpty()) {
        return ValidationResult::failure(QStringLiteral("container ID is empty"));
    }

    QSet<QString> structuralIds{m_id};
    QSet<QString> windowIds;
    for (qsizetype index = 0; index < m_pages.size(); ++index) {
        const auto &candidate = m_pages[index];
        if (candidate.id().isEmpty()) {
            return ValidationResult::failure(
                QStringLiteral("page %1 has an empty ID").arg(index));
        }
        if (structuralIds.contains(candidate.id())) {
            return ValidationResult::failure(
                QStringLiteral("duplicate structural ID '%1'").arg(candidate.id()));
        }
        structuralIds.insert(candidate.id());
        const auto result = candidate.root().validate(
            structuralIds,
            windowIds,
            QStringLiteral("page[%1]/root").arg(candidate.id()));
        if (!result.valid) {
            return result;
        }
    }

    if (m_pages.isEmpty()) {
        return m_activePageId.isEmpty()
            ? ValidationResult::success()
            : ValidationResult::failure(QStringLiteral("empty container has an active page"));
    }
    if (m_activePageId.isEmpty() || !page(m_activePageId)) {
        return ValidationResult::failure(QStringLiteral("active page ID is missing or unknown"));
    }
    return ValidationResult::success();
}

} // namespace QindaQt::Core
