// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/shell/status_notifier/item_client/status_notifier_item_monitor.h>
#include "status_notifier_item_menu.h"
#include <limits>

namespace QindaQt::StatusNotifier {
quint64 StatusNotifierItemMonitor::nextMenuRevision() {
    if (m_menuRevision == std::numeric_limits<quint64>::max()) {
        return 0;
    }
    return ++m_menuRevision;
}

bool StatusNotifierItemMonitor::itemIsMenu(const OwnerKey &target) const {
    const ItemSlot *slot = nullptr;
    return validateIntentForDispatch(target, RequestKind::ContextMenu, &slot).accepted()
        && slot->menu->itemIsMenu();
}

bool StatusNotifierItemMonitor::hasExportedMenu(const OwnerKey &target) const {
    const ItemSlot *slot = nullptr;
    return validateIntentForDispatch(target, RequestKind::ContextMenu, &slot).accepted()
        && slot->menu->hasExportedMenu();
}

QVariantMap StatusNotifierItemMonitor::menuState(const OwnerKey &target) const {
    const ItemSlot *slot = nullptr;
    if (!validateIntentForDispatch(target, RequestKind::ContextMenu, &slot).accepted()) {
        return {{QStringLiteral("status"), QStringLiteral("none")},
                {QStringLiteral("revision"), QStringLiteral("0")},
                {QStringLiteral("entries"), QVariantList{}}};
    }
    return slot->menu->state();
}

RegistryOutcome StatusNotifierItemMonitor::openMenu(const OwnerKey &target, int x, int y) {
    const ItemSlot *slot = nullptr;
    const auto outcome = validateIntentForDispatch(target, RequestKind::ContextMenu, &slot);
    if (!outcome.accepted()) {
        return outcome;
    }
    if (!slot->menu->descriptorCurrent()) {
        return {RegistryStatus::InvalidRequest, QStringLiteral("menu-descriptor-pending")};
    }
    if (!slot->menu->hasExportedMenu()) {
        slot->client->contextMenu(x, y);
        return outcome;
    }
    return slot->menu->open();
}

RegistryOutcome StatusNotifierItemMonitor::aboutToShowMenu(const OwnerKey &target,
                                                           quint64 revision, int id) {
    const ItemSlot *slot = nullptr;
    const auto result = validateIntentForDispatch(target, RequestKind::ContextMenu, &slot);
    return result.accepted() ? slot->menu->aboutToShow(revision, id) : result;
}

RegistryOutcome StatusNotifierItemMonitor::invokeMenu(const OwnerKey &target,
                                                      quint64 revision, int id) {
    const ItemSlot *slot = nullptr;
    const auto result = validateIntentForDispatch(target, RequestKind::ContextMenu, &slot);
    return result.accepted() ? slot->menu->invoke(revision, id) : result;
}
} // namespace QindaQt::StatusNotifier
