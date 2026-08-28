// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/ownership/invocation_guard.h>

#include <qindaqt/shell/global_menu/protocol/menu_item_lookup.h>

#include <optional>

namespace QindaQt::Shell::GlobalMenu::Ownership
{

InvocationResult InvocationGuard::evaluate(const ActiveProviderSelector &selector,
                                            const Protocol::MenuTree &tree,
                                            const InvocationRequest &request)
{
    const std::optional<SelectedProvider> current = selector.current();
    if (!current) {
        return InvocationResult{.accepted = false, .reasonCode = QStringLiteral("no-active-provider")};
    }
    if (current->window.windowId != request.windowId || current->epoch != request.epoch) {
        return InvocationResult{.accepted = false, .reasonCode = QStringLiteral("stale-owner")};
    }
    // AGENT-GUARD: the request and the presented tree must both carry the
    // selector's current lineage. Skipping this comparison would let a caller
    // authorize an id against content the current owner never published.
    if (tree.ownerWindowId != current->window.windowId || tree.epoch != current->epoch) {
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
