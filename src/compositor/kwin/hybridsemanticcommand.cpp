// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridsemanticcommand.h"

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

std::optional<HybridSemanticRequest> reject(QString *error, QString message)
{
    fail(error, std::move(message));
    return std::nullopt;
}

void collectWindowIds(const Core::LayoutNode &node, QStringList *result)
{
    if (node.isLeaf()) {
        result->append(node.windowId());
        return;
    }
    // Valid topology splits always have both children. Keep resolution total so
    // a malformed diagnostic snapshot is rejected by request validation rather
    // than dereferenced in an accessibility or shortcut callback.
    if (node.firstChild()) {
        collectWindowIds(*node.firstChild(), result);
    }
    if (node.secondChild()) {
        collectWindowIds(*node.secondChild(), result);
    }
}

std::optional<HybridChrome::WindowAction> actionFor(
    HybridSemanticCommand command)
{
    using enum HybridSemanticCommand;
    switch (command) {
    case CloseGroup:
        return HybridChrome::WindowAction::Close;
    case MinimizeGroup:
        return HybridChrome::WindowAction::Minimize;
    case MaximizeGroup:
        return HybridChrome::WindowAction::Maximize;
    case RestoreGroup:
        return HybridChrome::WindowAction::Restore;
    case BeginPageDock:
    case ActivateNextPage:
    case ActivatePreviousPage:
    case ReorderPageNext:
    case ReorderPagePrevious:
        return std::nullopt;
    }
    return std::nullopt;
}

} // namespace

bool HybridSemanticRequest::isValid(QString *error) const
{
    if (error) {
        error->clear();
    }
    if (containerId.isEmpty()) {
        return fail(error, QStringLiteral("semantic request has no container"));
    }
    switch (kind) {
    case HybridSemanticRequestKind::BeginPageDock:
        if (windowAction || pageId.isEmpty()
            || destinationPageIndex != -1
            || dockSource.kind != HybridInput::HitKind::Tab
            || dockSource.containerId != containerId
            || dockSource.pageId != pageId || !dockSource.isValid()) {
            return fail(error, QStringLiteral("page-dock request has an invalid tab source"));
        }
        return true;
    case HybridSemanticRequestKind::ActivatePage:
        if (windowAction || dockSource.isValid() || pageId.isEmpty()
            || destinationPageIndex != -1) {
            return fail(error, QStringLiteral("page activation request is inconsistent"));
        }
        return true;
    case HybridSemanticRequestKind::ReorderPage:
        if (windowAction || dockSource.isValid() || pageId.isEmpty()
            || destinationPageIndex < 0) {
            return fail(error, QStringLiteral("page reorder request is inconsistent"));
        }
        return true;
    case HybridSemanticRequestKind::GroupWindowAction:
        if (!windowAction || !pageId.isEmpty() || dockSource.isValid()
            || destinationPageIndex != -1) {
            return fail(error, QStringLiteral("group action request is inconsistent"));
        }
        return true;
    }
    return fail(error, QStringLiteral("unknown semantic request kind"));
}

std::optional<HybridSemanticRequest> HybridSemanticCommandResolver::resolveActive(
    const Hybrid::WindowTopology &topology,
    const QString &activeWindowId,
    HybridSemanticCommand command,
    QString *error)
{
    if (error) {
        error->clear();
    }
    const auto validation = topology.validate();
    if (!validation.valid) {
        return reject(error, QStringLiteral("invalid topology: %1").arg(validation.message));
    }
    if (activeWindowId.isEmpty()) {
        return reject(error, QStringLiteral("semantic command needs an active window"));
    }
    const auto owner = topology.ownerOf(activeWindowId);
    if (!owner) {
        return reject(error, topology.isIndependent(activeWindowId)
                                 ? QStringLiteral("semantic command requires a grouped window")
                                 : QStringLiteral("active window is absent from topology"));
    }
    const auto *container = topology.container(*owner);
    const auto *activePage = container
        ? container->page(container->activePageId()) : nullptr;
    if (!container || !activePage) {
        return reject(error, QStringLiteral("active group has no active page"));
    }

    if (const auto action = actionFor(command)) {
        return groupWindowAction(topology, *owner, *action, error);
    }
    if (command == HybridSemanticCommand::BeginPageDock) {
        QStringList members;
        collectWindowIds(activePage->root(), &members);
        if (members.isEmpty()) {
            return reject(error, QStringLiteral("active page has no member representative"));
        }
        const QString representative = members.contains(activeWindowId)
            ? activeWindowId : members.constFirst();
        HybridInput::HitTarget source{HybridInput::HitKind::Tab,
                                      *owner, representative, {}};
        source.pageId = activePage->id();
        HybridSemanticRequest request{
            .kind = HybridSemanticRequestKind::BeginPageDock,
            .containerId = *owner,
            .pageId = activePage->id(),
            .destinationPageIndex = -1,
            .dockSource = std::move(source),
            .windowAction = std::nullopt,
        };
        return request.isValid(error)
            ? std::optional<HybridSemanticRequest>(std::move(request))
            : std::nullopt;
    }

    const auto &pages = container->pages();
    if (pages.size() < 2) {
        return reject(error, QStringLiteral("active group has no alternate page"));
    }
    qsizetype activeIndex = -1;
    for (qsizetype index = 0; index < pages.size(); ++index) {
        if (pages[index].id() == container->activePageId()) {
            activeIndex = index;
            break;
        }
    }
    if (activeIndex < 0) {
        return reject(error, QStringLiteral("active page is absent from page order"));
    }
    const bool forward = command == HybridSemanticCommand::ActivateNextPage
        || command == HybridSemanticCommand::ReorderPageNext;
    const qsizetype destination = forward
        ? (activeIndex + 1) % pages.size()
        : (activeIndex + pages.size() - 1) % pages.size();
    if (command == HybridSemanticCommand::ReorderPageNext
        || command == HybridSemanticCommand::ReorderPagePrevious) {
        return reorderPage(topology, *owner, activePage->id(), destination, error);
    }
    return activatePage(topology, *owner, pages[destination].id(), error);
}

std::optional<HybridSemanticRequest> HybridSemanticCommandResolver::activatePage(
    const Hybrid::WindowTopology &topology,
    const QString &containerId,
    const QString &pageId,
    QString *error)
{
    if (error) {
        error->clear();
    }
    const auto *container = topology.container(containerId);
    if (!container || !container->page(pageId)) {
        return reject(error, QStringLiteral("unknown page '%1' in group '%2'")
                                 .arg(pageId, containerId));
    }
    HybridSemanticRequest request{
        .kind = HybridSemanticRequestKind::ActivatePage,
        .containerId = containerId,
        .pageId = pageId,
        .destinationPageIndex = -1,
        .dockSource = {},
        .windowAction = std::nullopt,
    };
    return request.isValid(error)
        ? std::optional<HybridSemanticRequest>(std::move(request))
        : std::nullopt;
}

std::optional<HybridSemanticRequest> HybridSemanticCommandResolver::groupWindowAction(
    const Hybrid::WindowTopology &topology,
    const QString &containerId,
    HybridChrome::WindowAction action,
    QString *error)
{
    if (error) {
        error->clear();
    }
    if (!topology.container(containerId)) {
        return reject(error, QStringLiteral("unknown window group '%1'").arg(containerId));
    }
    HybridSemanticRequest request{
        .kind = HybridSemanticRequestKind::GroupWindowAction,
        .containerId = containerId,
        .pageId = {},
        .destinationPageIndex = -1,
        .dockSource = {},
        .windowAction = action,
    };
    return request.isValid(error)
        ? std::optional<HybridSemanticRequest>(std::move(request))
        : std::nullopt;
}

std::optional<HybridSemanticRequest> HybridSemanticCommandResolver::reorderPage(
    const Hybrid::WindowTopology &topology,
    const QString &containerId,
    const QString &pageId,
    qsizetype destinationPageIndex,
    QString *error)
{
    if (error) {
        error->clear();
    }
    const auto *container = topology.container(containerId);
    if (!container || !container->page(pageId)
        || destinationPageIndex < 0
        || destinationPageIndex >= container->pages().size()) {
        return reject(error, QStringLiteral("invalid destination for page '%1' in group '%2'")
                                 .arg(pageId, containerId));
    }
    qsizetype sourceIndex = -1;
    for (qsizetype index = 0; index < container->pages().size(); ++index) {
        if (container->pages()[index].id() == pageId) {
            sourceIndex = index;
            break;
        }
    }
    if (sourceIndex == destinationPageIndex) {
        return reject(error, QStringLiteral("page is already at destination index"));
    }
    HybridSemanticRequest request{
        .kind = HybridSemanticRequestKind::ReorderPage,
        .containerId = containerId,
        .pageId = pageId,
        .destinationPageIndex = destinationPageIndex,
        .dockSource = {},
        .windowAction = std::nullopt,
    };
    return request.isValid(error)
        ? std::optional<HybridSemanticRequest>(std::move(request))
        : std::nullopt;
}

HybridSemanticCommandDispatcher::HybridSemanticCommandDispatcher(
    HybridSemanticCommandHandlers handlers)
    : m_handlers(std::move(handlers))
{
}

bool HybridSemanticCommandDispatcher::dispatch(
    const HybridSemanticRequest &request, QString *error) const
{
    if (error) {
        error->clear();
    }
    if (!request.isValid(error)) {
        return false;
    }
    bool accepted = false;
    switch (request.kind) {
    case HybridSemanticRequestKind::BeginPageDock:
        if (!m_handlers.beginPageDock) {
            return fail(error, QStringLiteral("page docking handler is unavailable"));
        }
        accepted = m_handlers.beginPageDock(request.dockSource, error);
        break;
    case HybridSemanticRequestKind::ActivatePage:
        if (!m_handlers.activatePage) {
            return fail(error, QStringLiteral("page activation handler is unavailable"));
        }
        accepted = m_handlers.activatePage(request.containerId, request.pageId, error);
        break;
    case HybridSemanticRequestKind::ReorderPage:
        if (!m_handlers.reorderPage) {
            return fail(error, QStringLiteral("page reorder handler is unavailable"));
        }
        accepted = m_handlers.reorderPage(request.containerId, request.pageId,
                                          request.destinationPageIndex, error);
        break;
    case HybridSemanticRequestKind::GroupWindowAction:
        if (!m_handlers.groupWindowAction) {
            return fail(error, QStringLiteral("group window-action handler is unavailable"));
        }
        accepted = m_handlers.groupWindowAction(
            request.containerId, *request.windowAction, error);
        break;
    }
    if (!accepted && error && error->isEmpty()) {
        *error = QStringLiteral("semantic command was rejected");
    }
    return accepted;
}

} // namespace QindaQt::Compositor::KWinIntegration
