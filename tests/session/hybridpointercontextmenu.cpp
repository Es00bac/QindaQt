// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridpointercontextmenu.h"

#include "hybridpointergrouping.h"

#include <QJsonDocument>

namespace QindaQt::Test {
namespace {

constexpr int InventoryTimeoutMilliseconds = 4000;

const ObservedWindow &window(const WindowInventory &inventory,
                             const QString &title)
{
    return inventory.constFind(title).value();
}

bool expectedLayerState(const WindowInventory &inventory,
                        const HybridPointerGroupedState &state,
                        bool keepAbove)
{
    const auto &source = window(inventory, state.gesture.sourceTitle);
    const auto &target = window(inventory, state.gesture.targetTitle);
    const auto &bystander = window(inventory, state.bystander);
    return source.containerId == state.publicContainer.containerId
        && target.containerId == state.publicContainer.containerId
        && source.keepAbove == keepAbove && target.keepAbove == keepAbove
        && !source.keepBelow && !target.keepBelow
        && !bystander.keepAbove && !bystander.keepBelow;
}

QString inventoryLayers(const WindowInventory &inventory,
                        const HybridPointerGroupedState &state)
{
    const auto encode = [&](const QString &title) {
        const auto &observed = window(inventory, title);
        return QJsonObject{{QStringLiteral("title"), title},
                           {QStringLiteral("owner"), observed.containerId},
                           {QStringLiteral("keepAbove"), observed.keepAbove},
                           {QStringLiteral("keepBelow"), observed.keepBelow}};
    };
    return QString::fromUtf8(
        QJsonDocument(QJsonObject{
            {QStringLiteral("source"), encode(state.gesture.sourceTitle)},
            {QStringLiteral("target"), encode(state.gesture.targetTitle)},
            {QStringLiteral("bystander"), encode(state.bystander)},
        }).toJson(QJsonDocument::Compact));
}

} // namespace

std::optional<HybridContextMenuEvidence> exerciseHybridContextMenu(
    CompositorProbeClient &client,
    HybridPointerGrouping &pointer,
    const HybridPointerGroupedState &state,
    const WindowInventory &raised,
    const QPointF &sharedTitlePoint,
    QString *error)
{
    const QStringList titles{state.gesture.sourceTitle,
                             state.gesture.targetTitle,
                             state.bystander};
    if (!state.output.contains(sharedTitlePoint)
        || !expectedLayerState(raised, state, false)) {
        *error = QStringLiteral(
            "group context-menu proof did not start on exposed unlayered chrome; point=%1 layers=%2")
                     .arg(QString::fromUtf8(
                              QJsonDocument(pointJson(sharedTitlePoint))
                                  .toJson(QJsonDocument::Compact)),
                          inventoryLayers(raised, state));
        return std::nullopt;
    }

    if (!pointer.activateFirstContextMenuAction(sharedTitlePoint, error)) {
        return std::nullopt;
    }
    auto keptAbove = client.awaitWindows(
        titles,
        [&](const WindowInventory &inventory) {
            return expectedLayerState(inventory, state, true);
        }, error, InventoryTimeoutMilliseconds);
    if (!keptAbove) {
        *error = QStringLiteral(
            "outer-title Keep Above did not propagate to every group member: %1")
                     .arg(*error);
        return std::nullopt;
    }

    // Production releases the captured command only after QMenu::hideEvent.
    // The member inventory above proves queued context adoption; this separate
    // publication gate proves stacking and anchored scene chrome are coherent
    // and addressable before the second real popup gesture.
    auto menuSettled = awaitHybridDiagnostics(
        client,
        [&](const HybridDiagnostics &value) {
            return value.revision == state.groupedHybrid.revision
                && value.containerCount == 1
                && value.json.value(
                       QStringLiteral("publishedGroupStackingCount"))
                       .toInt(-1) == 1
                && value.json.value(
                       QStringLiteral("quarantinedContainerCount"))
                       .toInt(-1) == 0
                && value.json.value(
                       QStringLiteral("visibleAnchoredChromeSceneItemCount"))
                       .toInt(-1) == 1;
        }, error);
    if (!menuSettled) {
        *error = QStringLiteral(
            "Keep Above menu closed without restoring addressable scene chrome: %1")
                     .arg(*error);
        return std::nullopt;
    }

    // Toggle the same first menu action back off. Besides leaving the live
    // fixture neutral for native detach, this proves the checked action is
    // rebuilt from the adopted group state rather than stale menu-local data.
    if (!pointer.activateFirstContextMenuAction(sharedTitlePoint, error)) {
        return std::nullopt;
    }
    auto restored = client.awaitWindows(
        titles,
        [&](const WindowInventory &inventory) {
            return expectedLayerState(inventory, state, false);
        }, error, InventoryTimeoutMilliseconds);
    if (!restored) {
        *error = QStringLiteral(
            "outer-title Keep Above did not toggle every group member back off: %1")
                     .arg(*error);
        return std::nullopt;
    }

    auto diagnostics = awaitHybridDiagnostics(
        client,
        [&](const HybridDiagnostics &value) {
            return value.revision == state.groupedHybrid.revision
                && value.containerCount == 1
                && value.json.value(QStringLiteral("chromeOverlayCount")).toInt(-1) == 1
                && value.json.value(
                       QStringLiteral("visibleAnchoredChromeSceneItemCount"))
                       .toInt(-1) == 1;
        }, error);
    const auto containers = client.containers(error);
    if (!diagnostics || !containers || containers->size() != 1
        || containers->at(0).toObject().value(QStringLiteral("id"))
            != state.publicContainer.containerId) {
        if (error->isEmpty()) {
            *error = QStringLiteral(
                "group context-menu toggles changed topology or lost scene chrome");
        }
        return std::nullopt;
    }
    return HybridContextMenuEvidence{*keptAbove, *restored,
                                     *diagnostics, sharedTitlePoint};
}

} // namespace QindaQt::Test
