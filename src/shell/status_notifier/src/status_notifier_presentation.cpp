// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/status_notifier/status_notifier_presentation.h>

#include <QtGlobal>

#include <algorithm>

namespace QindaQt::StatusNotifier
{
namespace
{

QString statusText(ItemStatus status)
{
    switch (status) {
    case ItemStatus::Passive:
        return QStringLiteral("passive");
    case ItemStatus::Active:
        return QStringLiteral("active");
    case ItemStatus::NeedsAttention:
        return QStringLiteral("needs attention");
    }
    return QStringLiteral("unknown");
}

QString descriptionFor(const ItemDescriptor &descriptor)
{
    if (!descriptor.toolTip.title.isEmpty()) {
        return descriptor.toolTip.title;
    }
    if (!descriptor.toolTip.description.isEmpty()) {
        return descriptor.toolTip.description;
    }
    return {};
}

// AGENT-NOTE: Activate and ContextMenu carry standard keyboard routes because
// the QML layer binds Enter/Space and Shift+F10/Application-key equivalents.
// SecondaryActivate is pointer-intent-only today; inventing a keyboard
// sequence here would mislead assistive technology, so the empty description
// is the truthful contract until a designed route exists.
QList<KeyboardAction> keyboardActionsFor()
{
    QList<KeyboardAction> actions;
    actions.append(KeyboardAction{RequestKind::Activate,
                                  QStringLiteral("Enter or Space")});
    actions.append(KeyboardAction{RequestKind::ContextMenu,
                                  QStringLiteral("Shift+F10 or Menu key")});
    actions.append(KeyboardAction{RequestKind::SecondaryActivate, {}});
    return actions;
}

} // namespace

TrayPresentation projectPresentation(const StatusNotifierRegistry &registry,
                                     const PresentationInput &input)
{
    TrayPresentation presentation;

    if (!input.transportLive) {
        presentation.state = PresentationState::Degraded;
        presentation.diagnostic = QStringLiteral("status-notifier-watcher-unavailable");
        return presentation;
    }

    if (registry.isDegraded()) {
        presentation.state = PresentationState::Degraded;
        presentation.diagnostic = registry.degradedReason();
    } else if (!registry.initialPopulationComplete()) {
        presentation.state = PresentationState::Loading;
        return presentation;
    } else if (registry.count() == 0) {
        presentation.state = PresentationState::Empty;
        presentation.diagnostic = QStringLiteral("no-status-notifier-items");
        return presentation;
    } else {
        presentation.state = PresentationState::Ready;
    }

    QList<OwnerKey> keys = registry.itemKeys();
    std::sort(keys.begin(), keys.end(), [](const OwnerKey &left, const OwnerKey &right) {
        if (left.uniqueName != right.uniqueName) {
            return left.uniqueName < right.uniqueName;
        }
        return left.objectPath < right.objectPath;
    });

    for (const OwnerKey &key : keys) {
        const std::optional<ItemDescriptor> descriptor = registry.find(key);
        if (!descriptor.has_value()) {
            continue;
        }

        TrayItemPresentation item;
        item.owner = key;
        item.identity = descriptor->identity;
        item.accessibleName = descriptor->title.isEmpty() ? descriptor->identity
                                                          : descriptor->title;
        item.accessibleDescription = descriptionFor(*descriptor);
        item.accessibleStatusText = statusText(descriptor->status);
        item.keyboardActions = keyboardActionsFor();
        presentation.items.append(item);
    }

    return presentation;
}

} // namespace QindaQt::StatusNotifier
