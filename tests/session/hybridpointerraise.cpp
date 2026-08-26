// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridpointerraise.h"

#include "compositorprobeclient.h"
#include "hybridpointergeometry.h"
#include "hybridpointergrouping.h"

#include <QJsonDocument>

#include <algorithm>
#include <array>
#include <cmath>

namespace QindaQt::Test {
namespace {

constexpr int InventoryTimeoutMilliseconds = 4000;

const ObservedWindow &window(const WindowInventory &inventory,
                             const QString &title)
{
    return inventory.constFind(title).value();
}

bool adjacentStackSlots(const ObservedWindow &first,
                        const ObservedWindow &second)
{
    return first.stackIndex >= 0 && second.stackIndex >= 0
        && std::abs(first.stackIndex - second.stackIndex) == 1;
}

QRectF inferredGroupOuterFrame(const ObservedWindow &first,
                               const ObservedWindow &second)
{
    // This live fixture uses the production default metrics: one outer border,
    // then 34px title and 34px tab rows above active member frames.
    return first.targetFrame.united(second.targetFrame)
        .adjusted(-1.0, -69.0, 1.0, 1.0);
}

std::optional<QPointF> exposedPoint(const QRectF &area,
                                    const QRectF &occluder,
                                    const QRectF &output)
{
    constexpr std::array<qreal, 7> fractions{
        0.50, 0.72, 0.28, 0.86, 0.14, 0.94, 0.06};
    for (const auto y : fractions) {
        for (const auto x : fractions) {
            const QPointF candidate(area.left() + area.width() * x,
                                    area.top() + area.height() * y);
            if (output.contains(candidate) && !occluder.contains(candidate)) {
                return candidate;
            }
        }
    }
    return std::nullopt;
}

QRectF sharedTitleRect(const ObservedWindow &first,
                       const ObservedWindow &second)
{
    const auto outer = inferredGroupOuterFrame(first, second);
    return QRectF(outer.left() + 8.0, outer.top() + 4.0,
                  outer.width() - 16.0, 26.0);
}

std::optional<QPointF> exposedSharedTitlePoint(
    const ObservedWindow &first,
    const ObservedWindow &second,
    const ObservedWindow &occluder,
    const QRectF &output)
{
    return exposedPoint(sharedTitleRect(first, second), occluder.frame, output);
}

std::optional<QPointF> coveredSharedTitlePoint(
    const ObservedWindow &first,
    const ObservedWindow &second,
    const ObservedWindow &occluder,
    const QRectF &output)
{
    auto covered = sharedTitleRect(first, second)
                       .intersected(occluder.frame)
                       .intersected(output);
    if (covered.width() < 4.0 || covered.height() < 4.0) {
        return std::nullopt;
    }
    covered.adjust(1.0, 1.0, -1.0, -1.0);
    return covered.center();
}

} // namespace

std::optional<RaisedGroupEvidence> coverAndRaiseGroup(
    CompositorProbeClient &client,
    HybridPointerGrouping &pointer,
    const HybridPointerGroupedState &state,
    const QStringList &probeTitles,
    const std::function<void(const QString &)> &activateProbe,
    const std::function<void(const QString &)> &showPopupForProbe,
    QString *error)
{
    const auto &sourceBefore = window(state.grouped, state.gesture.sourceTitle);
    const auto &targetBefore = window(state.grouped, state.gesture.targetTitle);
    if (!adjacentStackSlots(sourceBefore, targetBefore)) {
        *error = QStringLiteral("grouped members did not form adjacent KWin stack slots");
        return std::nullopt;
    }
    if (!activateProbe || !showPopupForProbe) {
        *error = QStringLiteral(
            "nested workflow omitted its owned-probe activation or popup seam");
        return std::nullopt;
    }

    activateProbe(state.bystander);
    const auto activated = client.awaitWindows(
        probeTitles,
        [&](const WindowInventory &inventory) {
            const auto &source = window(inventory, state.gesture.sourceTitle);
            const auto &target = window(inventory, state.gesture.targetTitle);
            const auto &bystander = window(inventory, state.bystander);
            return adjacentStackSlots(source, target) && bystander.active
                && bystander.stackIndex > std::max(source.stackIndex,
                                                   target.stackIndex);
        }, error, InventoryTimeoutMilliseconds);
    if (!activated) {
        *error = QStringLiteral("unrelated window did not rise above the compact group: %1")
                     .arg(*error);
        return std::nullopt;
    }

    const auto titleArea = sharedTitleRect(
        window(*activated, state.gesture.sourceTitle),
        window(*activated, state.gesture.targetTitle));
    const auto &moveCandidate = window(*activated, state.bystander);
    const QPointF movePress(moveCandidate.frame.center().x(),
                            moveCandidate.frame.top() + 12.0);
    const QPointF moveDrop = movePress
        + (titleArea.center() - moveCandidate.frame.center());
    if (!state.output.contains(movePress) || !state.output.contains(moveDrop)
        || !pointer.drag(movePress, moveDrop, false, error)) {
        if (error->isEmpty()) {
            *error = QStringLiteral("could not move the bystander over shared chrome");
        }
        return std::nullopt;
    }
    const auto covered = client.awaitWindows(
        probeTitles,
        [&](const WindowInventory &inventory) {
            const auto &source = window(inventory, state.gesture.sourceTitle);
            const auto &target = window(inventory, state.gesture.targetTitle);
            const auto &moved = window(inventory, state.bystander);
            return adjacentStackSlots(source, target) && moved.active
                && moved.stackIndex > std::max(source.stackIndex,
                                               target.stackIndex)
                && sharedTitleRect(source, target).intersects(moved.frame);
        }, error, InventoryTimeoutMilliseconds);
    if (!covered) {
        *error = QStringLiteral("unrelated window did not cover the compact group: %1")
                     .arg(*error);
        return std::nullopt;
    }

    const auto coveredPoint = coveredSharedTitlePoint(
        window(*covered, state.gesture.sourceTitle),
        window(*covered, state.gesture.targetTitle),
        window(*covered, state.bystander), state.output);
    if (!coveredPoint || !pointer.drag(*coveredPoint, *coveredPoint, false, error)) {
        if (error->isEmpty()) {
            *error = QStringLiteral(
                "bystander does not overlap the shared-title regression region");
        }
        return std::nullopt;
    }
    const auto stillCovered = client.awaitWindows(
        probeTitles,
        [&](const WindowInventory &inventory) {
            const auto &source = window(inventory, state.gesture.sourceTitle);
            const auto &target = window(inventory, state.gesture.targetTitle);
            const auto &bystander = window(inventory, state.bystander);
            return adjacentStackSlots(source, target) && bystander.active
                && bystander.stackIndex > std::max(source.stackIndex,
                                                   target.stackIndex)
                && source.containerId == target.containerId;
        }, error, InventoryTimeoutMilliseconds);
    if (!stillCovered) {
        *error = QStringLiteral(
            "ordinary click tunneled through a covering window into shared chrome: %1")
                     .arg(*error);
        return std::nullopt;
    }
    QString diagnosticsError;
    const auto coveredDiagnostics = readHybridDiagnostics(client, &diagnosticsError);
    if (!coveredDiagnostics
        || coveredDiagnostics->revision != state.groupedHybrid.revision
        || coveredDiagnostics->containerCount != 1) {
        *error = QStringLiteral(
            "ordinary click tunneled through a covering window into shared chrome: %1")
                     .arg(diagnosticsError);
        return std::nullopt;
    }

    const auto titlePoint = exposedSharedTitlePoint(
        window(*stillCovered, state.gesture.sourceTitle),
        window(*stillCovered, state.gesture.targetTitle),
        window(*stillCovered, state.bystander), state.output);
    if (!titlePoint) {
        *error = QStringLiteral("covered group has no exposed shared-title point");
        return std::nullopt;
    }
    showPopupForProbe(state.bystander);
    processProbeEventsFor(250);
    if (!pointer.drag(*titlePoint, *titlePoint, false, error)) {
        return std::nullopt;
    }
    const auto popupDismissed = client.awaitWindows(
        probeTitles,
        [&](const WindowInventory &inventory) {
            const auto &source = window(inventory, state.gesture.sourceTitle);
            const auto &target = window(inventory, state.gesture.targetTitle);
            const auto &bystander = window(inventory, state.bystander);
            return adjacentStackSlots(source, target) && bystander.active
                && bystander.stackIndex > std::max(source.stackIndex,
                                                   target.stackIndex)
                && source.containerId == target.containerId;
        }, error, InventoryTimeoutMilliseconds);
    QString popupDiagnosticsError;
    const auto popupDiagnostics = readHybridDiagnostics(
        client, &popupDiagnosticsError);
    if (!popupDismissed || !popupDiagnostics
        || popupDiagnostics->revision != state.groupedHybrid.revision
        || popupDiagnostics->containerCount != 1) {
        *error = QStringLiteral(
            "popup-dismiss press also reached shared chrome: %1; %2")
                     .arg(*error, popupDiagnosticsError);
        return std::nullopt;
    }
    if (!pointer.drag(*titlePoint, *titlePoint, false, error)) {
        return std::nullopt;
    }
    QString raisedError;
    const auto raised = client.awaitWindows(
        probeTitles,
        [&](const WindowInventory &inventory) {
            const auto &source = window(inventory, state.gesture.sourceTitle);
            const auto &target = window(inventory, state.gesture.targetTitle);
            const auto &bystander = window(inventory, state.bystander);
            return adjacentStackSlots(source, target)
                && (source.active != target.active)
                && std::min(source.stackIndex, target.stackIndex)
                    > bystander.stackIndex;
        }, &raisedError, InventoryTimeoutMilliseconds);
    if (!raised) {
        *error = QStringLiteral(
            "shared chrome did not raise and activate its group: %1; point=%2")
                     .arg(raisedError,
                          QString::fromUtf8(QJsonDocument(pointJson(*titlePoint))
                                                .toJson(QJsonDocument::Compact)));
        return std::nullopt;
    }
    const auto diagnostics = awaitHybridDiagnostics(
        client,
        [](const HybridDiagnostics &value) {
            return value.containerCount == 1
                && value.json.value(QStringLiteral("chromeOverlayCount")).toInt(-1) == 1
                && value.json.value(
                       QStringLiteral("visibleAnchoredChromeSceneItemCount"))
                       .toInt(-1) == 1;
        }, error);
    if (!diagnostics) {
        *error = QStringLiteral("raised group lost its scene item: %1").arg(*error);
        return std::nullopt;
    }
    return RaisedGroupEvidence{*stillCovered, *raised, *diagnostics,
                               *coveredPoint, *titlePoint};
}

} // namespace QindaQt::Test
