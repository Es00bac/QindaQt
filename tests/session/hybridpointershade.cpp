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

    // Unroll: same menu, same index (only the label flips), same point (the
    // shared row's own position never moved).
    if (!pointer.activateContextMenuActionAt(
            sharedTitlePoint, RollUpMenuActionIndex, error)) {
        return std::nullopt;
    }
    auto unrolled = client.awaitWindows(
        titles,
        [&](const WindowInventory &inventory) {
            return bothMembersMatch(inventory, grouped, state)
                && bothMembersHidden(inventory, state, /*expectedHidden=*/false);
        }, error, InventoryTimeoutMilliseconds);
    if (!unrolled) {
        *error = QStringLiteral(
            "member frames did not remain exact, or members were not "
            "restored to visible, after unrolling the group: %1")
                     .arg(*error);
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

    return HybridPointerShadeEvidence{*shaded, *unrolled,
                                      *shadedDiagnostics, *unrolledDiagnostics};
}

} // namespace QindaQt::Test
