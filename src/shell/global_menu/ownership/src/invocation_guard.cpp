// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/ownership/invocation_guard.h>

#include <qindaqt/shell/global_menu/protocol/menu_item_lookup.h>

#include <optional>

namespace QindaQt::Shell::GlobalMenu::Ownership
{

namespace
{

// AGENT-GUARD: request, tree, and selector must carry one identical lineage.
// Dropping any of the three comparisons lets an invocation authorize against
// a window, epoch, or revision the current owner never published.
bool sameLineage(const QUuid &windowId, const QUuid &epoch, quint64 revision,
                 const SelectedProvider &current)
{
    return current.window.windowId == windowId && current.epoch == epoch
        && current.revision == revision;
}

} // namespace

InvocationResult InvocationGuard::evaluate(const ActiveProviderSelector &selector,
                                           const Protocol::MenuTree &tree,
                                           const InvocationRequest &request)
{
    const std::optional<SelectedProvider> current = selector.current();
    if (!current) {
        return InvocationResult{.accepted = false, .reasonCode = QStringLiteral("no-active-provider")};
    }
    if (!sameLineage(request.windowId, request.epoch, request.revision, *current)
        || !sameLineage(tree.ownerWindowId, tree.epoch, tree.revision, *current)) {
        return InvocationResult{.accepted = false, .reasonCode = QStringLiteral("stale-owner")};
    }

    const Protocol::MenuItem *item = Protocol::findMenuItemById(tree.items, request.actionId);
    if (!item) {
        return InvocationResult{.accepted = false, .reasonCode = QStringLiteral("unknown-action")};
    }
    if (item->kind != Protocol::MenuItemKind::Action) {
        return InvocationResult{.accepted = false, .reasonCode = QStringLiteral("not-invocable")};
    }
    if (!item->enabled || !item->visible) {
        return InvocationResult{.accepted = false, .reasonCode = QStringLiteral("disabled")};
    }

    return InvocationResult{.accepted = true, .reasonCode = QString()};
}

} // namespace QindaQt::Shell::GlobalMenu::Ownership
