// SPDX-License-Identifier: LGPL-3.0-or-later
#include "layoutnode.h"

#include <cmath>
#include <utility>

namespace QindaQt::Core {

LayoutNode::LayoutNode(QString nodeId, QString windowId)
    : m_kind(Kind::Leaf)
    , m_id(std::move(nodeId))
    , m_windowId(std::move(windowId))
{
}

LayoutNode::LayoutNode(QString nodeId,
                       SplitOrientation orientation,
                       double ratio,
                       LayoutNode first,
                       LayoutNode second)
    : m_kind(Kind::Split)
    , m_id(std::move(nodeId))
    , m_orientation(orientation)
    , m_ratio(ratio)
    , m_first(std::make_unique<LayoutNode>(std::move(first)))
    , m_second(std::make_unique<LayoutNode>(std::move(second)))
{
}

LayoutNode::LayoutNode(const LayoutNode &other)
    : m_kind(other.m_kind)
    , m_id(other.m_id)
    , m_windowId(other.m_windowId)
    , m_orientation(other.m_orientation)
    , m_ratio(other.m_ratio)
    , m_first(other.m_first ? std::make_unique<LayoutNode>(*other.m_first) : nullptr)
    , m_second(other.m_second ? std::make_unique<LayoutNode>(*other.m_second) : nullptr)
{
}

LayoutNode::LayoutNode(LayoutNode &&other) noexcept = default;

LayoutNode &LayoutNode::operator=(const LayoutNode &other)
{
    if (this != &other) {
        LayoutNode copy(other);
        *this = std::move(copy);
    }
    return *this;
}

LayoutNode &LayoutNode::operator=(LayoutNode &&other) noexcept = default;
LayoutNode::~LayoutNode() = default;

LayoutNode LayoutNode::makeLeaf(QString nodeId, QString windowId)
{
    return LayoutNode(std::move(nodeId), std::move(windowId));
}

LayoutNode LayoutNode::makeSplit(QString nodeId,
                                 SplitOrientation orientation,
                                 double ratio,
                                 LayoutNode first,
                                 LayoutNode second)
{
    return LayoutNode(std::move(nodeId),
                      orientation,
                      ratio,
                      std::move(first),
                      std::move(second));
}

std::optional<SplitOrientation> LayoutNode::orientation() const noexcept
{
    return isSplit() ? std::optional(m_orientation) : std::nullopt;
}

std::optional<double> LayoutNode::ratio() const noexcept
{
    return isSplit() ? std::optional(m_ratio) : std::nullopt;
}

const LayoutNode *LayoutNode::findNode(const QString &nodeId) const noexcept
{
    if (m_id == nodeId) {
        return this;
    }
    if (isLeaf()) {
        return nullptr;
    }
    if (const auto *match = m_first->findNode(nodeId)) {
        return match;
    }
    return m_second->findNode(nodeId);
}

const LayoutNode *LayoutNode::findWindow(const QString &windowId) const noexcept
{
    if (isLeaf()) {
        return m_windowId == windowId ? this : nullptr;
    }
    if (const auto *match = m_first->findWindow(windowId)) {
        return match;
    }
    return m_second->findWindow(windowId);
}

LayoutNode *LayoutNode::findWindowMutable(const QString &windowId) noexcept
{
    if (isLeaf()) {
        return m_windowId == windowId ? this : nullptr;
    }
    if (auto *match = m_first->findWindowMutable(windowId)) {
        return match;
    }
    return m_second->findWindowMutable(windowId);
}

bool LayoutNode::splitWindow(const QString &targetWindowId,
                             LayoutNode newLeaf,
                             QString splitNodeId,
                             SplitOrientation orientation,
                             double ratio,
                             InsertPosition position)
{
    if (isLeaf()) {
        if (m_windowId != targetWindowId) {
            return false;
        }

        // AGENT-GUARD: Move the existing leaf below the new split instead of
        // rewriting it. Its node ID is an externally persisted stable handle.
        LayoutNode existingLeaf(std::move(*this));
        if (position == InsertPosition::First) {
            *this = makeSplit(std::move(splitNodeId),
                              orientation,
                              ratio,
                              std::move(newLeaf),
                              std::move(existingLeaf));
        } else {
            *this = makeSplit(std::move(splitNodeId),
                              orientation,
                              ratio,
                              std::move(existingLeaf),
                              std::move(newLeaf));
        }
        return true;
    }

    return m_first->splitWindow(targetWindowId,
                                LayoutNode(newLeaf),
                                splitNodeId,
                                orientation,
                                ratio,
                                position)
        || m_second->splitWindow(targetWindowId,
                                 std::move(newLeaf),
                                 std::move(splitNodeId),
                                 orientation,
                                 ratio,
                                 position);
}

bool LayoutNode::setSplitRatio(const QString &splitNodeId, double ratio) noexcept
{
    if (isSplit() && m_id == splitNodeId) {
        m_ratio = ratio;
        return true;
    }
    if (isLeaf()) {
        return false;
    }
    return m_first->setSplitRatio(splitNodeId, ratio)
        || m_second->setSplitRatio(splitNodeId, ratio);
}

LayoutNode::RemovalOutcome LayoutNode::removeWindow(const QString &windowId,
                                                    QString *removedLeafNodeId)
{
    if (isLeaf()) {
        if (m_windowId != windowId) {
            return RemovalOutcome::NotFound;
        }
        if (removedLeafNodeId) {
            *removedLeafNodeId = m_id;
        }
        return RemovalOutcome::RemoveThisNode;
    }

    const auto firstOutcome = m_first->removeWindow(windowId, removedLeafNodeId);
    if (firstOutcome == RemovalOutcome::RemoveThisNode) {
        // AGENT-NOTE: A binary split with one child is meaningless. Promoting
        // the survivor also preserves every stable ID still in the model.
        // Move through a local: assigning directly from an owned child would
        // destroy that child midway through LayoutNode's move assignment.
        LayoutNode survivor(std::move(*m_second));
        *this = std::move(survivor);
        return RemovalOutcome::RemovedDescendant;
    }
    if (firstOutcome == RemovalOutcome::RemovedDescendant) {
        return firstOutcome;
    }

    const auto secondOutcome = m_second->removeWindow(windowId, removedLeafNodeId);
    if (secondOutcome == RemovalOutcome::RemoveThisNode) {
        LayoutNode survivor(std::move(*m_first));
        *this = std::move(survivor);
        return RemovalOutcome::RemovedDescendant;
    }
    return secondOutcome;
}

ValidationResult LayoutNode::validate(QSet<QString> &structuralIds,
                                      QSet<QString> &windowIds,
                                      const QString &path) const
{
    if (m_id.isEmpty()) {
        return ValidationResult::failure(path + QStringLiteral(": node ID is empty"));
    }
    if (structuralIds.contains(m_id)) {
        return ValidationResult::failure(path + QStringLiteral(": duplicate structural ID '")
                                         + m_id + QStringLiteral("'"));
    }
    structuralIds.insert(m_id);

    if (isLeaf()) {
        if (m_windowId.isEmpty()) {
            return ValidationResult::failure(path + QStringLiteral(": window ID is empty"));
        }
        if (windowIds.contains(m_windowId)) {
            return ValidationResult::failure(path + QStringLiteral(": duplicate window ID '")
                                             + m_windowId + QStringLiteral("'"));
        }
        windowIds.insert(m_windowId);
        if (m_first || m_second) {
            return ValidationResult::failure(path + QStringLiteral(": leaf has child nodes"));
        }
        return ValidationResult::success();
    }

    if (!m_windowId.isEmpty()) {
        return ValidationResult::failure(path + QStringLiteral(": split has a window ID"));
    }
    if (!m_first || !m_second) {
        return ValidationResult::failure(path + QStringLiteral(": split requires two children"));
    }
    if (!std::isfinite(m_ratio) || m_ratio <= 0.0 || m_ratio >= 1.0) {
        return ValidationResult::failure(path + QStringLiteral(": ratio must be between 0 and 1"));
    }

    const auto firstResult = m_first->validate(structuralIds,
                                               windowIds,
                                               path + QStringLiteral("/first"));
    if (!firstResult.valid) {
        return firstResult;
    }
    return m_second->validate(structuralIds,
                              windowIds,
                              path + QStringLiteral("/second"));
}

} // namespace QindaQt::Core
