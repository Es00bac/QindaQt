// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridtaskidentitypolicy.h"

#include <QSet>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

bool collectLeaves(const Core::LayoutNode &node,
                   QStringList *windowIds,
                   QString *error)
{
    if (node.isLeaf()) {
        if (node.windowId().isEmpty()) {
            return fail(error, QStringLiteral("task identity contains an empty window ID"));
        }
        windowIds->append(node.windowId());
        return true;
    }
    if (!node.firstChild() || !node.secondChild()) {
        return fail(error, QStringLiteral("task identity contains an incomplete split"));
    }
    return collectLeaves(*node.firstChild(), windowIds, error)
        && collectLeaves(*node.secondChild(), windowIds, error);
}

} // namespace

const TaskMemberIdentity *TaskContainerIdentity::member(
    const QString &windowId) const noexcept
{
    for (const auto &candidate : members) {
        if (candidate.windowId == windowId) {
            return &candidate;
        }
    }
    return nullptr;
}

bool TaskContainerIdentity::isValid(QString *error) const
{
    if (containerId.isEmpty() || activePageId.isEmpty()
        || primaryWindowId.isEmpty() || members.isEmpty()) {
        return fail(error, QStringLiteral(
                               "task identity needs container, active page, primary, and members"));
    }
    QSet<QString> ids;
    qsizetype primaryCount = 0;
    for (const auto &candidate : members) {
        if (candidate.containerId != containerId || candidate.pageId.isEmpty()
            || candidate.windowId.isEmpty() || ids.contains(candidate.windowId)) {
            return fail(error, QStringLiteral("task identity member ownership is invalid"));
        }
        ids.insert(candidate.windowId);
        if (candidate.primary) {
            ++primaryCount;
            if (!candidate.activePage || candidate.windowId != primaryWindowId
                || candidate.skipTaskbar || candidate.skipSwitcher) {
                return fail(error, QStringLiteral("task primary is not an exposed active-page member"));
            }
        } else if (!candidate.skipTaskbar || !candidate.skipSwitcher) {
            return fail(error, QStringLiteral("task secondary is not fully suppressed"));
        }
        if (candidate.activePage != (candidate.pageId == activePageId)) {
            return fail(error, QStringLiteral("task member active-page state is inconsistent"));
        }
    }
    if (primaryCount != 1 || !member(primaryWindowId)) {
        return fail(error, QStringLiteral("task identity must expose exactly one primary"));
    }
    return true;
}

std::optional<TaskContainerIdentity> HybridTaskIdentityPolicy::planContainer(
    const Core::WindowContainer &container,
    const QString &preferredActiveWindowId,
    QString *error)
{
    if (error) {
        error->clear();
    }
    const auto validation = container.validate();
    const auto *activePage = container.page(container.activePageId());
    if (!validation.valid || !activePage) {
        fail(error, validation.valid
                        ? QStringLiteral("task container has no active page")
                        : validation.message);
        return std::nullopt;
    }

    QStringList activeIds;
    if (!collectLeaves(activePage->root(), &activeIds, error) || activeIds.isEmpty()) {
        return std::nullopt;
    }
    const QString primaryId = activeIds.contains(preferredActiveWindowId)
        ? preferredActiveWindowId : activeIds.first();

    TaskContainerIdentity result{
        .containerId = container.id(),
        .activePageId = container.activePageId(),
        .primaryWindowId = primaryId,
        .members = {},
    };
    QSet<QString> seen;
    for (const auto &page : container.pages()) {
        QStringList ids;
        if (!collectLeaves(page.root(), &ids, error)) {
            return std::nullopt;
        }
        for (const auto &windowId : ids) {
            if (seen.contains(windowId)) {
                fail(error, QStringLiteral("task identity repeats window '%1'").arg(windowId));
                return std::nullopt;
            }
            seen.insert(windowId);
            const bool primary = windowId == primaryId;
            result.members.append({
                .containerId = container.id(),
                .pageId = page.id(),
                .windowId = windowId,
                .activePage = page.id() == container.activePageId(),
                .primary = primary,
                .skipTaskbar = !primary,
                .skipSwitcher = !primary,
            });
        }
    }
    if (!result.isValid(error)) {
        return std::nullopt;
    }
    return result;
}

std::optional<QVector<TaskContainerIdentity>> HybridTaskIdentityPolicy::planTopology(
    const Hybrid::WindowTopology &topology,
    const QString &preferredActiveWindowId,
    QString *error)
{
    if (error) {
        error->clear();
    }
    const auto validation = topology.validate();
    if (!validation.valid) {
        fail(error, validation.message);
        return std::nullopt;
    }
    QVector<TaskContainerIdentity> result;
    result.reserve(topology.containerIds().size());
    for (const auto &containerId : topology.containerIds()) {
        const auto *container = topology.container(containerId);
        if (!container) {
            fail(error, QStringLiteral("task topology lost container '%1'").arg(containerId));
            return std::nullopt;
        }
        auto plan = planContainer(*container, preferredActiveWindowId, error);
        if (!plan) {
            return std::nullopt;
        }
        result.append(std::move(*plan));
    }
    return result;
}

const TaskMemberIdentity *HybridTaskIdentityPolicy::findMember(
    const QVector<TaskContainerIdentity> &plans,
    const QString &windowId) noexcept
{
    for (const auto &plan : plans) {
        if (const auto *candidate = plan.member(windowId)) {
            return candidate;
        }
    }
    return nullptr;
}

TaskIdentityDecision HybridTaskIdentityPolicy::decide(
    const TaskMemberIdentity *member,
    TaskIdentityEvent event)
{
    if (!member) {
        return {};
    }
    if (!member->activePage
        && (event == TaskIdentityEvent::Activated
            || event == TaskIdentityEvent::Unminimized)) {
        return {
            .action = TaskIdentityAction::ActivatePage,
            .containerId = member->containerId,
            .pageId = member->pageId,
            .windowId = member->windowId,
            .hideBeforeAction = true,
        };
    }
    if (member->activePage && event == TaskIdentityEvent::Minimized) {
        return {
            .action = TaskIdentityAction::MinimizeContainer,
            .containerId = member->containerId,
            .pageId = member->pageId,
            .windowId = member->windowId,
        };
    }
    return {};
}

} // namespace QindaQt::Compositor::KWinIntegration
