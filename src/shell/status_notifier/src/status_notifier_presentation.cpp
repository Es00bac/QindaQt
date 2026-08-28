// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/status_notifier/status_notifier_presentation.h>

#include <algorithm>

namespace QindaQt::StatusNotifier
{
namespace
{

QString statusText(ItemStatus status, const PresentationTexts &texts)
{
    switch (status) {
    case ItemStatus::Passive:
        return texts.statusPassive;
    case ItemStatus::Active:
        return texts.statusActive;
    case ItemStatus::NeedsAttention:
        return texts.statusNeedsAttention;
    }
    return texts.statusUnknown;
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

// AGENT-NOTE: The keyboard descriptions are injected localized text, not a
// binding that exists today: the status tray applet still resolves as
// implementation-unavailable (see applet-runtime.md). When the tray presenter
// lands it will bind Activate and ContextMenu to Enter/Space and
// Shift+F10/Application-key equivalents. SecondaryActivation is
// pointer-intent-only by design; inventing a keyboard sequence here would
// mislead assistive technology, so the empty text is the truthful contract.
QList<KeyboardAction> keyboardActionsFor(const PresentationTexts &texts)
{
    QList<KeyboardAction> actions;
    actions.append(KeyboardAction{RequestKind::Activate, texts.keyboardActivate});
    actions.append(KeyboardAction{RequestKind::ContextMenu, texts.keyboardContextMenu});
    actions.append(KeyboardAction{RequestKind::SecondaryActivate,
                                  texts.keyboardSecondaryActivate});
    return actions;
}

void projectItems(const StatusNotifierRegistry &registry, const PresentationTexts &texts,
                  TrayPresentation &presentation)
{
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
        // Validation guarantees a nonblank title or none at all, so the
        // fallback keeps every projected item nonblank-accessible.
        item.accessibleName = descriptor->title.isEmpty() ? descriptor->identity
                                                          : descriptor->title;
        item.accessibleDescription = descriptionFor(*descriptor);
        item.accessibleStatusText = statusText(descriptor->status, texts);
        item.keyboardActions = keyboardActionsFor(texts);
        presentation.items.append(item);
    }
}

} // namespace

TrayPresentation projectPresentation(const StatusNotifierRegistry &registry,
                                     const PresentationInput &input,
                                     const PresentationTexts &texts)
{
    TrayPresentation presentation;

    if (!input.transportLive) {
        // AGENT-GUARD: Watcher loss keeps the last-known-good items visible
        // and actionable per the accepted Degraded contract (ADR-0032);
        // their owners are still live on the bus, and blanking the tray here
        // would contradict the documented failure behavior.
        presentation.state = PresentationState::Degraded;
        presentation.diagnostic = QStringLiteral("status-notifier-watcher-unavailable");
        projectItems(registry, texts, presentation);
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

    projectItems(registry, texts, presentation);
    return presentation;
}

} // namespace QindaQt::StatusNotifier
