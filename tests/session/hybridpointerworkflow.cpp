// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridpointerworkflow.h"

#include "compositordevelopmentworkflow.h"
#include "compositorprobeclient.h"
#include "hybridpointergeometry.h"
#include "hybridpointergrouping.h"
#include "hybridpointerinventory.h"
#include "hybridpointerraise.h"
#include "hybridcompositorrestart.h"
#include "hybridpointercontextmenu.h"
#include "hybridtestinputdriver.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace QindaQt::Test {
namespace {

constexpr int InventoryTimeoutMilliseconds = 4000;

const ObservedWindow &window(const WindowInventory &inventory,
                             const QString &title)
{
    return inventory.constFind(title).value();
}

QJsonObject windowEvidence(const ObservedWindow &observed)
{
    return {{QStringLiteral("id"), observed.id},
            {QStringLiteral("owner"), observed.containerId},
            {QStringLiteral("active"), observed.active},
            {QStringLiteral("keepAbove"), observed.keepAbove},
            {QStringLiteral("keepBelow"), observed.keepBelow},
            {QStringLiteral("stackIndex"), observed.stackIndex},
            {QStringLiteral("frame"), rectJson(observed.frame)},
            {QStringLiteral("targetFrame"), rectJson(observed.targetFrame)}};
}

bool adjacentStackSlots(const ObservedWindow &first,
                        const ObservedWindow &second)
{
    return first.stackIndex >= 0 && second.stackIndex >= 0
        && std::abs(first.stackIndex - second.stackIndex) == 1;
}

bool sameExtent(const QRectF &first, const QRectF &second)
{
    return nearlyEqual(first.width(), second.width())
        && nearlyEqual(first.height(), second.height());
}

bool nativeMoveReached(const ObservedWindow &current,
                       const ObservedWindow &before,
                       const QPointF &dropPoint)
{
    // AGENT-CONTRACT: KWin preserves the pointer's normalized horizontal
    // anchor when the topology transaction restores the member's independent
    // size. The press is twelve logical pixels below the native title edge.
    // This proves the titlebar continued its ordinary interactive move after
    // QindaQt detached the member instead of merely restoring it in place.
    const QPointF titleAnchor(current.frame.center().x(),
                              current.frame.top() + 12.0);
    return sameExtent(current.frame, before.frame)
        && sameGeometry(current.frame, current.targetFrame)
        && !sameGeometry(current.frame, before.frame)
        && nearlyEqual(titleAnchor.x(), dropPoint.x())
        && nearlyEqual(titleAnchor.y(), dropPoint.y());
}

bool establishFloatingTransient(
    CompositorProbeClient &client,
    const HybridPointerGroupedState &state,
    const QStringList &probeTitles,
    const std::function<QString(const QString &)> &showDialogForProbe,
    QString *error)
{
    if (!showDialogForProbe) {
        *error = QStringLiteral("nested workflow omitted its transient-dialog seam");
        return false;
    }
    // AGENT-GUARD: Docking makes the target the stable activation/task
    // representative. Own the dialog from the other member so a shared-title
    // raise must preserve transient-aware stacking independently of focus.
    // Regressing to "representative is always top" splits this live block.
    const auto dialogTitle = showDialogForProbe(state.gesture.sourceTitle);
    if (dialogTitle.isEmpty()) {
        *error = QStringLiteral("nested workflow could not create its transient dialog");
        return false;
    }
    processProbeEventsFor(250);
    const auto managed = client.windows(error);
    if (!managed) {
        return false;
    }
    for (const auto &entry : *managed) {
        if (entry.toObject().value(QStringLiteral("title")).toString()
            == dialogTitle) {
            *error = QStringLiteral(
                "normal-type transient dialog entered the manageable topology inventory");
            return false;
        }
    }
    const auto dialogFocused = client.awaitWindows(
        probeTitles,
        [&](const WindowInventory &inventory) {
            const auto &source = window(inventory, state.gesture.sourceTitle);
            const auto &target = window(inventory, state.gesture.targetTitle);
            return adjacentStackSlots(source, target)
                && std::none_of(inventory.cbegin(), inventory.cend(),
                                [](const ObservedWindow &observed) {
                                    return observed.active;
                                });
        }, error, InventoryTimeoutMilliseconds);
    if (!dialogFocused) {
        *error = QStringLiteral(
            "floating transient did not retain focus outside Hybrid topology: %1")
                     .arg(*error);
        return false;
    }
    return true;
}

struct NativeDetachEvidence final
{
    WindowInventory restored;
    HybridDiagnostics diagnostics;
    QPointF memberTitlePoint;
    QPointF emptyDesktopPoint;
};

std::optional<NativeDetachEvidence> detachNativeMember(
    CompositorProbeClient &client,
    HybridPointerGrouping &pointer,
    const HybridPointerGroupedState &state,
    const QStringList &probeTitles,
    const WindowInventory &raised,
    QString *error)
{
    const auto &groupedSource = window(raised, state.gesture.sourceTitle);
    const QPointF memberPress(groupedSource.targetFrame.center().x(),
                              groupedSource.targetFrame.top() + 12.0);
    const auto emptyPoint = emptyDesktopPoint(
        raised, {state.gesture.sourceTitle, state.gesture.targetTitle},
        state.output, error);
    if (!state.output.contains(memberPress) || !emptyPoint) {
        if (error->isEmpty()) {
            *error = QStringLiteral(
                "grouped member chrome has no safe detach gesture geometry");
        }
        return std::nullopt;
    }
    if (!pointer.drag(memberPress, *emptyPoint, false, error)) {
        return std::nullopt;
    }

    const auto restored = client.awaitWindows(
        probeTitles,
        [&](const WindowInventory &inventory) {
            const auto &dragged = window(inventory, state.gesture.sourceTitle);
            const auto &draggedBefore = window(state.initial,
                                               state.gesture.sourceTitle);
            const auto &sibling = window(inventory, state.gesture.targetTitle);
            const auto &siblingBefore = window(state.initial,
                                               state.gesture.targetTitle);
            return dragged.containerId.isEmpty() && !dragged.minimized
                && nativeMoveReached(dragged, draggedBefore, *emptyPoint)
                && sibling.containerId.isEmpty() && !sibling.minimized
                && sameGeometry(sibling.frame, siblingBefore.frame)
                && sameGeometry(sibling.targetFrame, siblingBefore.targetFrame)
                && window(inventory, state.bystander).containerId.isEmpty();
        }, error, InventoryTimeoutMilliseconds);
    if (!restored) {
        *error = QStringLiteral(
            "native member-title drag did not detach and move its member: %1; %2")
                     .arg(*error, pointer.dotoolDiagnostics());
        return std::nullopt;
    }
    const auto diagnostics = awaitHybridDiagnostics(
        client,
        [&](const HybridDiagnostics &value) {
            return value.revision > state.groupedHybrid.revision
                && value.containerCount == 0
                && value.json.value(QStringLiteral("chromeOverlayCount")).toInt(-1) == 0
                && value.json.value(
                       QStringLiteral("visibleAnchoredChromeSceneItemCount"))
                       .toInt(-1) == 0;
        }, error);
    const auto containers = client.containers(error);
    if (!diagnostics || !containers || !containers->isEmpty()
        || !pointer.dotoolRunning()) {
        if (error->isEmpty()) {
            *error = !pointer.dotoolRunning()
                ? QStringLiteral("dotool exited before the workflow completed; %1")
                      .arg(pointer.dotoolDiagnostics())
                : QStringLiteral("Containers retained a normalized singleton owner");
        }
        return std::nullopt;
    }
    return NativeDetachEvidence{*restored, *diagnostics,
                                memberPress, *emptyPoint};
}

QJsonObject workflowEvidence(const HybridPointerGroupedState &state,
                             const HybridPointerGrouping &pointer,
                             const HybridCompositorRestartEvidence &restart,
                             const RaisedGroupEvidence &raised,
                             const HybridContextMenuEvidence &contextMenu,
                             const NativeDetachEvidence &detached)
{
    const auto &initialSource = window(state.grouped, state.gesture.sourceTitle);
    const auto &initialTarget = window(state.grouped, state.gesture.targetTitle);
    auto evidence = pointer.inputEvidence();
    evidence.insert(QStringLiteral("workflow"), QStringLiteral("hybrid-pointer"));
    evidence.insert(QStringLiteral("exactModifierGesture"),
                    QStringLiteral("Meta+Shift+Left"));
    evidence.insert(QStringLiteral("dropZone"), state.gesture.dropZone);
    evidence.insert(QStringLiteral("sourceTitle"), state.gesture.sourceTitle);
    evidence.insert(QStringLiteral("targetTitle"), state.gesture.targetTitle);
    evidence.insert(QStringLiteral("sourcePoint"), pointJson(state.gesture.sourcePoint));
    evidence.insert(QStringLiteral("dropPoint"), pointJson(state.gesture.dropPoint));
    evidence.insert(QStringLiteral("memberTitlePoint"),
                    pointJson(detached.memberTitlePoint));
    evidence.insert(QStringLiteral("coveredSharedChromePoint"),
                    pointJson(raised.coveredSharedTitlePoint));
    evidence.insert(QStringLiteral("emptyDesktopPoint"),
                    pointJson(detached.emptyDesktopPoint));
    evidence.insert(QStringLiteral("groupContextMenuPoint"),
                    pointJson(contextMenu.sharedTitlePoint));
    evidence.insert(QStringLiteral("topologyRevisionAdvanced"),
                    state.initialHybrid.revision < state.groupedHybrid.revision
                        && state.groupedHybrid.revision < detached.diagnostics.revision);
    evidence.insert(QStringLiteral("compositorRestartObserved"), true);
    evidence.insert(QStringLiteral("sceneChromeRepublishedAfterRestart"), true);
    evidence.insert(
        QStringLiteral("compositorRestart"),
        QJsonObject{{QStringLiteral("topologyRevision"), restart.topologyRevision},
                    {QStringLiteral("containerRevision"), restart.containerRevision},
                    {QStringLiteral("sameHybridRevision"), true},
                    {QStringLiteral("sameContainer"), true},
                    {QStringLiteral("chromeOverlayCount"), 1},
                    {QStringLiteral("visibleAnchoredChromeSceneItemCount"), 1}});
    for (const auto &key : {
             "sameOwnerAfterDock", "hybridSnapshotReadable",
             "plainNativeDecorationDetach", "nativeDecorationMemberTitleDrag",
             "ownersClearedAfterDetach", "draggedMemberFollowedPointer",
             "draggedMemberSizeRestored", "siblingExactFrameRestored",
             "chromeSceneAttached", "chromeSceneRemovedAfterDetach",
             "groupStackContiguous", "unrelatedWindowCoveredGroupChrome",
             "coveredWindowBlockedSharedChromeInput",
             "popupGrabDismissedBeforeSharedChromeInput",
             "normalTransientExcludedFromTopology",
             "transientFocusPreservedOutsideTopology",
             "unrelatedWindowRemainedAboveTransientGroup",
             "sharedChromeRaisedGroupUnit", "stableGroupRepresentativeActivated",
             "outerTitleContextMenuOpened", "productionQMenuKeepAboveTriggered",
             "groupContextQueuedAdoption", "keepAboveAppliedToEveryMember",
             "keepAboveToggleRecoveredEveryMember"}) {
        evidence.insert(QString::fromLatin1(key), true);
    }
    evidence.insert(QStringLiteral("validTargetSplit"), state.split.valid);
    evidence.insert(QStringLiteral("publicContainerRevision"),
                    state.publicContainer.revision);
    evidence.insert(QStringLiteral("publicSnapshot"), state.publicContainer.snapshot);
    evidence.insert(QStringLiteral("splitOrientation"), state.split.orientation);
    evidence.insert(QStringLiteral("dividerGap"), state.split.dividerGap);
    evidence.insert(QStringLiteral("unrelatedWindowInventoried"),
                    window(state.grouped, state.bystander).containerId.isEmpty());
    evidence.insert(
        QStringLiteral("initial"),
        QJsonObject{{QStringLiteral("hybrid"), state.initialHybrid.json},
                    {QStringLiteral("source"),
                     windowEvidence(window(state.initial, state.gesture.sourceTitle))},
                    {QStringLiteral("target"),
                     windowEvidence(window(state.initial, state.gesture.targetTitle))}});
    evidence.insert(
        QStringLiteral("grouped"),
        QJsonObject{{QStringLiteral("hybrid"), state.groupedHybrid.json},
                    {QStringLiteral("source"), windowEvidence(initialSource)},
                    {QStringLiteral("target"), windowEvidence(initialTarget)}});
    evidence.insert(
        QStringLiteral("covered"),
        QJsonObject{{QStringLiteral("hybrid"), state.groupedHybrid.json},
                    {QStringLiteral("source"), windowEvidence(window(
                         raised.covered, state.gesture.sourceTitle))},
                    {QStringLiteral("target"), windowEvidence(window(
                         raised.covered, state.gesture.targetTitle))},
                    {QStringLiteral("bystander"), windowEvidence(window(
                         raised.covered, state.bystander))}});
    evidence.insert(
        QStringLiteral("raised"),
        QJsonObject{{QStringLiteral("hybrid"), raised.diagnostics.json},
                    {QStringLiteral("source"), windowEvidence(window(
                         raised.raised, state.gesture.sourceTitle))},
                    {QStringLiteral("target"), windowEvidence(window(
                         raised.raised, state.gesture.targetTitle))},
                    {QStringLiteral("bystander"), windowEvidence(window(
                         raised.raised, state.bystander))}});
    evidence.insert(
        QStringLiteral("groupContextMenu"),
        QJsonObject{
            {QStringLiteral("point"), pointJson(contextMenu.sharedTitlePoint)},
            {QStringLiteral("keptAboveSource"),
             windowEvidence(window(contextMenu.keptAbove,
                                   state.gesture.sourceTitle))},
            {QStringLiteral("keptAboveTarget"),
             windowEvidence(window(contextMenu.keptAbove,
                                   state.gesture.targetTitle))},
            {QStringLiteral("restoredSource"),
             windowEvidence(window(contextMenu.restored,
                                   state.gesture.sourceTitle))},
            {QStringLiteral("restoredTarget"),
             windowEvidence(window(contextMenu.restored,
                                   state.gesture.targetTitle))},
            {QStringLiteral("topologyRevision"),
             QString::number(contextMenu.diagnostics.revision)}});
    evidence.insert(
        QStringLiteral("restored"),
        QJsonObject{{QStringLiteral("hybrid"), detached.diagnostics.json},
                    {QStringLiteral("source"), windowEvidence(window(
                         detached.restored, state.gesture.sourceTitle))},
                    {QStringLiteral("target"), windowEvidence(window(
                         detached.restored, state.gesture.targetTitle))}});
    return evidence;
}

} // namespace

std::optional<HybridPointerWorkflowResult> exerciseHybridPointerWorkflow(
    CompositorProbeClient &client,
    const ProbeWindowTitles &titles,
    const QString &dotoolPath,
    const std::function<void(const QString &)> &activateProbe,
    const std::function<void(const QString &)> &showPopupForProbe,
    const std::function<QString(const QString &)> &showDialogForProbe,
    QString *error)
{
    HybridPointerGrouping pointer(client, titles);
    const auto state = pointer.group(dotoolPath, error);
    if (!state) {
        return std::nullopt;
    }
    const auto restart = exerciseHybridCompositorRestart(client, *state, error);
    if (!restart) {
        return std::nullopt;
    }
    const QStringList probeTitles{titles.primary, titles.secondary, titles.page};
    if (!establishFloatingTransient(
            client, *state, probeTitles, showDialogForProbe, error)) {
        return std::nullopt;
    }
    const auto raised = coverAndRaiseGroup(
        client, pointer, *state, probeTitles,
        activateProbe, showPopupForProbe, error);
    if (!raised) {
        return std::nullopt;
    }
    const auto contextMenu = exerciseHybridContextMenu(
        client, pointer, *state, raised->raised,
        raised->sharedTitlePoint, error);
    if (!contextMenu) {
        return std::nullopt;
    }
    const auto detached = detachNativeMember(
        client, pointer, *state, probeTitles, contextMenu->restored, error);
    if (!detached) {
        return std::nullopt;
    }
    return HybridPointerWorkflowResult{
        workflowEvidence(*state, pointer, *restart, *raised,
                         *contextMenu, *detached),
        detached->diagnostics.json};
}

} // namespace QindaQt::Test
