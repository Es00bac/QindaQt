// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridpointershade.h"

#include "hybridpointergrouping.h"

namespace QindaQt::Test {
namespace {

constexpr int InventoryTimeoutMilliseconds = 4000;
// Menu order in KWinGroupContextMenu::prepare(): Arrange windows(0),
// Detach active window(1), Ungroup(2), Minimize group(3), Roll up/Unroll
// group(4). The label toggles with state but the index never moves.
constexpr int RollUpMenuActionIndex = 4;

const ObservedWindow &window(const WindowInventory &inventory,
                             const QString &title)
{
    return inventory.constFind(title).value();
}

bool bothMembersMatch(const WindowInventory &inventory,
                      const WindowInventory &expected,
                      const HybridPointerGroupedState &state)
{
    const auto &source = window(inventory, state.gesture.sourceTitle);
    const auto &expectedSource = window(expected, state.gesture.sourceTitle);
    const auto &target = window(inventory, state.gesture.targetTitle);
    const auto &expectedTarget = window(expected, state.gesture.targetTitle);
    return sameGeometry(source.frame, expectedSource.frame)
        && sameGeometry(source.targetFrame, expectedSource.targetFrame)
        && sameGeometry(target.frame, expectedTarget.frame)
        && sameGeometry(target.targetFrame, expectedTarget.targetFrame);
}

// AGENT-CONTRACT: proves the actual KWin::Window::isHidden() state each
// member carries, not just that the compositor believes it shaded a
// container. hidden is distinct from minimized (ADR-0099): shade must never
// set the real minimized bit, since that would also collapse the container
// to the dock and re-trigger native-minimize-to-whole-container-minimize
// interception.
bool bothMembersHidden(const WindowInventory &inventory,
                       const HybridPointerGroupedState &state,
                       bool expectedHidden)
{
    const auto &source = window(inventory, state.gesture.sourceTitle);
    const auto &target = window(inventory, state.gesture.targetTitle);
    return source.hidden == expectedHidden && !source.minimized
        && target.hidden == expectedHidden && !target.minimized;
}

// Unroll-at-a-dragged-position reflows the whole container to the strip's
// new origin at its original (pre-shade) size, so ratios/size are unchanged
// and every member frame simply translates by the same drag delta.
bool bothMembersMatchTranslated(const WindowInventory &inventory,
                                const WindowInventory &expected,
                                const HybridPointerGroupedState &state,
                                const QPointF &delta)
{
    const auto &source = window(inventory, state.gesture.sourceTitle);
    const auto &expectedSource = window(expected, state.gesture.sourceTitle);
    const auto &target = window(inventory, state.gesture.targetTitle);
    const auto &expectedTarget = window(expected, state.gesture.targetTitle);
    return sameGeometry(source.frame, expectedSource.frame.translated(delta))
        && sameGeometry(source.targetFrame,
                        expectedSource.targetFrame.translated(delta))
        && sameGeometry(target.frame, expectedTarget.frame.translated(delta))
        && sameGeometry(target.targetFrame,
                        expectedTarget.targetFrame.translated(delta));
}

std::optional<QRectF> soleShadedStripFrame(const HybridDiagnostics &diagnostics,
                                           QString *error)
{
    const auto frames = diagnostics.json.value(QStringLiteral("shadedStripFrames"))
                             .toArray();
    if (frames.size() != 1) {
        *error = QStringLiteral(
            "expected exactly one shaded strip frame, found %1").arg(frames.size());
        return std::nullopt;
    }
    const auto frame = frames.first().toObject();
    return QRectF(frame.value(QStringLiteral("x")).toDouble(),
                 frame.value(QStringLiteral("y")).toDouble(),
                 frame.value(QStringLiteral("width")).toDouble(),
                 frame.value(QStringLiteral("height")).toDouble());
}

} // namespace

std::optional<HybridPointerShadeEvidence> exerciseHybridPointerShade(
    CompositorProbeClient &client,
    HybridPointerGrouping &pointer,
    const HybridPointerGroupedState &state,
    const WindowInventory &grouped,
    const QPointF &sharedTitlePoint,
    QString *error)
{
    const QStringList titles{state.gesture.sourceTitle,
                             state.gesture.targetTitle, state.bystander};

    // AGENT-CONTRACT: this is the exact claim ADR-0099 depends on. Shading
    // must never move or resize either real member window; only compositor
    // bookkeeping (shadedContainerCount) may change.
    if (!pointer.activateContextMenuActionAt(
            sharedTitlePoint, RollUpMenuActionIndex, error)) {
        return std::nullopt;
    }
    auto shaded = client.awaitWindows(
        titles,
        [&](const WindowInventory &inventory) {
            return bothMembersMatch(inventory, grouped, state)
                && bothMembersHidden(inventory, state, /*expectedHidden=*/true);
        }, error, InventoryTimeoutMilliseconds);
    if (!shaded) {
        *error = QStringLiteral(
            "member frames changed, or members were not genuinely hidden, "
            "while shading the group: %1").arg(*error);
        return std::nullopt;
    }
    auto shadedDiagnostics = awaitHybridDiagnostics(
        client,
        [&](const HybridDiagnostics &value) {
            return value.containerCount == 1
                && value.json.value(QStringLiteral("shadedContainerCount"))
                       .toInt(-1) == 1;
        }, error);
    if (!shadedDiagnostics) {
        *error = QStringLiteral(
            "compositor did not report the container as shaded: %1").arg(*error);
        return std::nullopt;
    }

    const auto shadedFrameBeforeDrag = soleShadedStripFrame(*shadedDiagnostics, error);
    if (!shadedFrameBeforeDrag) {
        return std::nullopt;
    }

    // Drag the rolled strip to a new position. Ordinary title-bar drag
    // semantics apply: the grabbed point stays under the pointer, so the new
    // shared-title click point is exactly sharedTitlePoint + dragDelta.
    constexpr QPointF dragDelta{40.0, 24.0};
    const QPointF draggedTitlePoint = sharedTitlePoint + dragDelta;
    if (!pointer.drag(sharedTitlePoint, draggedTitlePoint, /*metaShift=*/false, error)) {
        return std::nullopt;
    }
    auto movedDiagnostics = awaitHybridDiagnostics(
        client,
        [&](const HybridDiagnostics &value) {
            const auto frame = soleShadedStripFrame(value, error);
            return frame
                && sameGeometry(*frame, shadedFrameBeforeDrag->translated(dragDelta));
        }, error);
    if (!movedDiagnostics) {
        *error = QStringLiteral(
            "dragging the shaded strip did not move its published frame by "
            "the exact drag delta: %1").arg(*error);
        return std::nullopt;
    }
    // Members stay exactly where they were before shading; only the
    // independent strip frame (and the compositor bookkeeping tracking it)
    // may move under drag.
    auto draggedMembers = client.awaitWindows(
        titles,
        [&](const WindowInventory &inventory) {
            return bothMembersMatch(inventory, grouped, state)
                && bothMembersHidden(inventory, state, /*expectedHidden=*/true);
        }, error, InventoryTimeoutMilliseconds);
    if (!draggedMembers) {
        *error = QStringLiteral(
            "member frames changed, or members were unhidden, while "
            "dragging the shaded strip: %1").arg(*error);
        return std::nullopt;
    }

    // Unroll at the dragged position: same menu, same index (only the label
    // flips), the strip's new position rather than its original one.
    if (!pointer.activateContextMenuActionAt(
            draggedTitlePoint, RollUpMenuActionIndex, error)) {
        return std::nullopt;
    }
    auto unrolled = client.awaitWindows(
        titles,
        [&](const WindowInventory &inventory) {
            return bothMembersMatchTranslated(inventory, grouped, state, dragDelta)
                && bothMembersHidden(inventory, state, /*expectedHidden=*/false);
        }, error, InventoryTimeoutMilliseconds);
    if (!unrolled) {
        *error = QStringLiteral(
            "member frames were not the pre-shade frames translated by the "
            "exact drag delta, or members were not restored to visible, "
            "after unrolling the dragged group: %1").arg(*error);
        return std::nullopt;
    }
    auto unrolledDiagnostics = awaitHybridDiagnostics(
        client,
        [&](const HybridDiagnostics &value) {
            return value.containerCount == 1
                && value.json.value(QStringLiteral("shadedContainerCount"))
                       .toInt(-1) == 0;
        }, error);
    if (!unrolledDiagnostics) {
        *error = QStringLiteral(
            "compositor did not clear shaded bookkeeping after unrolling: %1")
                     .arg(*error);
        return std::nullopt;
    }

    // AGENT-GUARD: the drag proof above is a real, permanent reposition of
    // the container (that is the point of it), but later phases of this same
    // probe run (e.g. the compositor-restart proof) still expect the group
    // at its original, pristine position. Drag the strip back by the exact
    // inverse delta and unroll there so this phase leaves the world exactly
    // as it found it, rather than leaking a moved container into unrelated
    // later coverage.
    if (!pointer.activateContextMenuActionAt(
            draggedTitlePoint, RollUpMenuActionIndex, error)) {
        return std::nullopt;
    }
    auto reshaded = client.awaitWindows(
        titles,
        [&](const WindowInventory &inventory) {
            return bothMembersMatchTranslated(inventory, grouped, state, dragDelta)
                && bothMembersHidden(inventory, state, /*expectedHidden=*/true);
        }, error, InventoryTimeoutMilliseconds);
    if (!reshaded) {
        *error = QStringLiteral(
            "could not re-shade the group to drag it back to its original "
            "position: %1").arg(*error);
        return std::nullopt;
    }
    if (!pointer.drag(draggedTitlePoint, sharedTitlePoint, /*metaShift=*/false, error)) {
        return std::nullopt;
    }
    auto restoredDiagnostics = awaitHybridDiagnostics(
        client,
        [&](const HybridDiagnostics &value) {
            const auto frame = soleShadedStripFrame(value, error);
            return frame && sameGeometry(*frame, *shadedFrameBeforeDrag);
        }, error);
    if (!restoredDiagnostics) {
        *error = QStringLiteral(
            "dragging the shaded strip back to its original position "
            "failed: %1").arg(*error);
        return std::nullopt;
    }
    if (!pointer.activateContextMenuActionAt(
            sharedTitlePoint, RollUpMenuActionIndex, error)) {
        return std::nullopt;
    }
    auto restored = client.awaitWindows(
        titles,
        [&](const WindowInventory &inventory) {
            return bothMembersMatch(inventory, grouped, state)
                && bothMembersHidden(inventory, state, /*expectedHidden=*/false);
        }, error, InventoryTimeoutMilliseconds);
    if (!restored) {
        *error = QStringLiteral(
            "member frames did not return to their exact pre-shade position "
            "after dragging the strip back and unrolling: %1").arg(*error);
        return std::nullopt;
    }

    return HybridPointerShadeEvidence{*shaded, *unrolled, *shadedDiagnostics,
                                      *movedDiagnostics, *unrolledDiagnostics,
                                      dragDelta};
}

} // namespace QindaQt::Test
