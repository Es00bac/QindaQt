// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridpointertaskidentityworkflow.h"

#include "compositorprobeclient.h"
#include "hybridpointergrouping.h"
#include "hybridpointerinventory.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QWindow>

#include <algorithm>

namespace QindaQt::Test {
namespace {

constexpr int InventoryTimeoutMilliseconds = 5000;

const ObservedWindow &window(const WindowInventory &inventory,
                             const QString &title)
{
    return inventory.constFind(title).value();
}

QWindow *windowByTitle(const QString &title,
                       QWindow &primary,
                       QWindow &secondary,
                       QWindow &page)
{
    for (auto *candidate : {&primary, &secondary, &page}) {
        if (candidate->title() == title) {
            return candidate;
        }
    }
    return nullptr;
}

bool hasOneCollapsedIdentity(const WindowInventory &inventory,
                             const QStringList &titles,
                             QString *primaryTitle = nullptr)
{
    QString taskPrimary;
    QString switcherPrimary;
    qsizetype taskEntries = 0;
    qsizetype switcherEntries = 0;
    for (const auto &title : titles) {
        const auto found = inventory.constFind(title);
        if (found == inventory.cend()) {
            return false;
        }
        if (!found->skipTaskbar) {
            ++taskEntries;
            taskPrimary = title;
        }
        if (!found->skipSwitcher) {
            ++switcherEntries;
            switcherPrimary = title;
        }
    }
    const bool collapsed = taskEntries == 1 && switcherEntries == 1
        && taskPrimary == switcherPrimary;
    if (collapsed && primaryTitle) {
        *primaryTitle = taskPrimary;
    }
    return collapsed;
}

QString activePageId(const PublicContainerEvidence &evidence)
{
    return evidence.snapshot.value(QStringLiteral("snapshot"))
        .toObject().value(QStringLiteral("activePageId")).toString();
}

QString compactJson(const QJsonValue &value)
{
    const auto document = value.isArray()
        ? QJsonDocument(value.toArray())
        : QJsonDocument(value.toObject());
    return QString::fromUtf8(document.toJson(QJsonDocument::Compact));
}

qsizetype pageCount(const PublicContainerEvidence &evidence)
{
    return evidence.snapshot.value(QStringLiteral("snapshot"))
        .toObject().value(QStringLiteral("pages")).toArray().size();
}

struct LifecycleProgress final
{
    QStringList splitTitles;
    QStringList groupedTitles;
    QWindow *pageClient = nullptr;
    QWindow *splitSourceClient = nullptr;
    QString splitPageId;
    QString splitPrimaryTitle;
    HybridDiagnostics regroupedHybrid;
    HybridDiagnostics activatedHybrid;
    HybridDiagnostics splitHybrid;
};

bool prepareLifecycle(QWindow &primary,
                      QWindow &secondary,
                      QWindow &page,
                      const HybridPointerGroupedState &state,
                      LifecycleProgress *progress,
                      QString *error)
{
    progress->splitTitles = {state.gesture.sourceTitle,
                             state.gesture.targetTitle};
    progress->groupedTitles = {state.gesture.sourceTitle,
                               state.gesture.targetTitle,
                               state.bystander};
    if (!hasOneCollapsedIdentity(state.grouped, progress->splitTitles)) {
        *error = QStringLiteral(
            "initial grouped split did not collapse to one task/switcher identity");
        return false;
    }
    progress->pageClient = windowByTitle(state.bystander,
                                         primary, secondary, page);
    progress->splitSourceClient = windowByTitle(state.gesture.sourceTitle,
                                                primary, secondary, page);
    progress->splitPageId = activePageId(state.publicContainer);
    if (!progress->pageClient || !progress->splitSourceClient
        || progress->splitPageId.isEmpty()) {
        *error = QStringLiteral("Hybrid page workflow lost its initial identity");
        return false;
    }
    return true;
}

bool regroupIndependentPage(HybridPointerGrouping &pointer,
                            const HybridPointerGroupedState &state,
                            CompositorProbeClient &client,
                            LifecycleProgress *progress,
                            QString *error)
{
    progress->pageClient->raise();
    progress->pageClient->requestActivate();
    const auto raised = client.awaitWindows(
        progress->groupedTitles,
        [&](const WindowInventory &inventory) {
            const auto &candidate = window(inventory, state.bystander);
            return candidate.containerId.isEmpty() && !candidate.minimized
                && candidate.active;
        }, error, InventoryTimeoutMilliseconds);
    if (!raised) {
        *error = QStringLiteral("could not activate the independent page source: %1")
                     .arg(*error);
        return false;
    }

    // AGENT-CONTRACT: Center regroup must use the explicit modified path. A
    // plain native title drag intentionally detaches and continues moving.
    const QPointF press(window(*raised, state.bystander).frame.center().x(),
                        window(*raised, state.bystander).frame.top() + 12.0);
    const QPointF drop(
        window(*raised, state.gesture.targetTitle).targetFrame.center());
    if (!state.output.contains(press) || !state.output.contains(drop)
        || !pointer.drag(press, drop, true, error)) {
        *error = QStringLiteral("Meta+Shift page regroup failed: %1").arg(*error);
        return false;
    }
    QString regroupError;
    const auto regrouped = client.awaitWindows(
        progress->groupedTitles,
        [&](const WindowInventory &inventory) {
            const auto &source = window(inventory, state.gesture.sourceTitle);
            const auto &target = window(inventory, state.gesture.targetTitle);
            const auto &inactive = window(inventory, state.bystander);
            return source.containerId == state.publicContainer.containerId
                && target.containerId == source.containerId
                && inactive.containerId == source.containerId
                && !source.minimized && !target.minimized && inactive.minimized
                && inactive.skipTaskbar && inactive.skipSwitcher
                && hasOneCollapsedIdentity(inventory, progress->groupedTitles);
        }, &regroupError, InventoryTimeoutMilliseconds);
    QString diagnosticsError;
    const auto diagnostics = awaitHybridDiagnostics(
        client,
        [&](const HybridDiagnostics &value) {
            return value.revision > state.groupedHybrid.revision
                && value.containerCount == 1;
        }, &diagnosticsError);
    QString publicError;
    const auto publicState = diagnostics
        ? readPublicHybridContainer(client, state.publicContainer.containerId,
                                    diagnostics->revision, &publicError)
        : std::nullopt;
    if (!regrouped || !diagnostics || !publicState
        || activePageId(*publicState) != progress->splitPageId
        || pageCount(*publicState) != 2) {
        QString inventoryError;
        const auto rawInventory = client.windows(&inventoryError);
        QString primaryTitle;
        QString currentError;
        const auto current = client.awaitWindows(
            progress->groupedTitles, [](const WindowInventory &) { return true; },
            &currentError, 100);
        const bool collapsedIdentity = current
            && hasOneCollapsedIdentity(*current, progress->groupedTitles,
                                       &primaryTitle);
        *error = QStringLiteral(
            "page regroup did not retain an excluded inactive page; "
            "sourceWindow=%1 targetWindow=%2 targetPage=%3 before=%4 after=%5 "
            "windows=%6 collapsedIdentity=%7 primary=%8 "
            "regroupError=%9 diagnosticsError=%10 publicError=%11 "
            "inventoryError=%12 currentError=%13")
                     .arg(window(*raised, state.bystander).id,
                          window(*raised, state.gesture.targetTitle).id,
                          progress->splitPageId,
                          compactJson(state.publicContainer.snapshot),
                          publicState ? compactJson(publicState->snapshot)
                                      : QStringLiteral("<none>"),
                          rawInventory ? compactJson(*rawInventory)
                                       : QStringLiteral("<none>"),
                          collapsedIdentity ? QStringLiteral("true")
                                            : QStringLiteral("false"),
                          primaryTitle,
                          regroupError,
                          diagnosticsError,
                          publicError,
                          inventoryError,
                          currentError);
        return false;
    }
    progress->regroupedHybrid = *diagnostics;
    return true;
}

bool activateInactivePage(const HybridPointerGroupedState &state,
                          CompositorProbeClient &client,
                          LifecycleProgress *progress,
                          QString *error)
{
    progress->pageClient->showNormal();
    progress->pageClient->requestActivate();
    const auto inventory = client.awaitWindows(
        progress->groupedTitles,
        [&](const WindowInventory &value) {
            const auto &source = window(value, state.gesture.sourceTitle);
            const auto &target = window(value, state.gesture.targetTitle);
            const auto &activated = window(value, state.bystander);
            return source.minimized && target.minimized
                && !activated.minimized && activated.active
                && !activated.skipTaskbar && !activated.skipSwitcher
                && hasOneCollapsedIdentity(value, progress->groupedTitles);
        }, error, InventoryTimeoutMilliseconds);
    const auto diagnostics = awaitHybridDiagnostics(
        client,
        [&](const HybridDiagnostics &value) {
            return value.revision > progress->regroupedHybrid.revision
                && value.containerCount == 1;
        }, error);
    const auto publicState = diagnostics
        ? readPublicHybridContainer(client, state.publicContainer.containerId,
                                    diagnostics->revision, error)
        : std::nullopt;
    if (!inventory || !diagnostics || !publicState
        || activePageId(*publicState).isEmpty()
        || activePageId(*publicState) == progress->splitPageId) {
        *error = QStringLiteral(
            "inactive member exposure did not activate its page atomically: %1")
                         .arg(*error);
        return false;
    }
    progress->activatedHybrid = *diagnostics;
    return true;
}

bool reactivateSplitPage(const HybridPointerGroupedState &state,
                         CompositorProbeClient &client,
                         LifecycleProgress *progress,
                         QString *error)
{
    progress->splitSourceClient->showNormal();
    progress->splitSourceClient->requestActivate();
    const auto inventory = client.awaitWindows(
        progress->groupedTitles,
        [&](const WindowInventory &value) {
            const auto &source = window(value, state.gesture.sourceTitle);
            const auto &target = window(value, state.gesture.targetTitle);
            const auto &inactive = window(value, state.bystander);
            return !source.minimized && !target.minimized && inactive.minimized
                && source.active && inactive.skipTaskbar && inactive.skipSwitcher
                && hasOneCollapsedIdentity(value, progress->groupedTitles);
        }, error, InventoryTimeoutMilliseconds);
    const auto diagnostics = awaitHybridDiagnostics(
        client,
        [&](const HybridDiagnostics &value) {
            return value.revision > progress->activatedHybrid.revision
                && value.containerCount == 1;
        }, error);
    const auto publicState = diagnostics
        ? readPublicHybridContainer(client, state.publicContainer.containerId,
                                    diagnostics->revision, error)
        : std::nullopt;
    if (!inventory || !diagnostics || !publicState
        || activePageId(*publicState) != progress->splitPageId
        || !hasOneCollapsedIdentity(*inventory, progress->groupedTitles,
                                    &progress->splitPrimaryTitle)) {
        *error = QStringLiteral(
            "split page reactivation exposed stale page members: %1").arg(*error);
        return false;
    }
    progress->splitHybrid = *diagnostics;
    return true;
}

bool minimizeAndRestore(QWindow &primary,
                        QWindow &secondary,
                        QWindow &page,
                        const HybridPointerGroupedState &state,
                        CompositorProbeClient &client,
                        const LifecycleProgress &progress,
                        QString *error)
{
    auto *const taskPrimary = windowByTitle(progress.splitPrimaryTitle,
                                            primary, secondary, page);
    if (!taskPrimary) {
        *error = QStringLiteral("collapsed task primary has no live probe client");
        return false;
    }
    taskPrimary->showMinimized();
    const auto minimized = client.awaitWindows(
        progress.groupedTitles,
        [&](const WindowInventory &inventory) {
            return std::all_of(inventory.cbegin(), inventory.cend(),
                               [](const ObservedWindow &observed) {
                                   return observed.minimized;
                               })
                && hasOneCollapsedIdentity(inventory, progress.groupedTitles);
        }, error, InventoryTimeoutMilliseconds);
    const auto minimizedDiagnostics = awaitHybridDiagnostics(
        client,
        [&](const HybridDiagnostics &value) {
            return value.revision == progress.splitHybrid.revision
                && value.containerCount == 1
                && value.json.value(
                       QStringLiteral("visibleAnchoredChromeSceneItemCount"))
                       .toInt(-1) == 0;
        }, error);
    if (!minimized || !minimizedDiagnostics) {
        *error = QStringLiteral(
            "native member minimize left a collapsed-layout hole: %1").arg(*error);
        return false;
    }

    taskPrimary->showNormal();
    taskPrimary->requestActivate();
    const auto restored = client.awaitWindows(
        progress.groupedTitles,
        [&](const WindowInventory &inventory) {
            const auto &source = window(inventory, state.gesture.sourceTitle);
            const auto &target = window(inventory, state.gesture.targetTitle);
            const auto &inactive = window(inventory, state.bystander);
            return !source.minimized && !target.minimized && inactive.minimized
                && hasOneCollapsedIdentity(inventory, progress.groupedTitles);
        }, error, InventoryTimeoutMilliseconds);
    const auto restoredDiagnostics = awaitHybridDiagnostics(
        client,
        [&](const HybridDiagnostics &value) {
            return value.revision == progress.splitHybrid.revision
                && value.containerCount == 1
                && value.json.value(
                       QStringLiteral("visibleAnchoredChromeSceneItemCount"))
                       .toInt(-1) == 1;
        }, error);
    if (!restored || !restoredDiagnostics) {
        *error = QStringLiteral(
            "collapsed restore exposed inactive-page members: %1").arg(*error);
        return false;
    }
    return true;
}

} // namespace

std::optional<HybridTaskIdentityLifecycleEvidence>
exerciseHybridTaskIdentityLifecycle(
    QWindow &primary,
    QWindow &secondary,
    QWindow &page,
    HybridPointerGrouping &pointer,
    const HybridPointerGroupedState &state,
    CompositorProbeClient &client,
    QString *error)
{
    LifecycleProgress progress;
    if (!prepareLifecycle(primary, secondary, page, state, &progress, error)
        || !regroupIndependentPage(pointer, state, client, &progress, error)
        || !activateInactivePage(state, client, &progress, error)
        || !reactivateSplitPage(state, client, &progress, error)
        || !minimizeAndRestore(primary, secondary, page, state, client,
                               progress, error)) {
        return std::nullopt;
    }
    return HybridTaskIdentityLifecycleEvidence{
        .regroupedRevision = progress.regroupedHybrid.revision,
        .inactivePageRevision = progress.activatedHybrid.revision,
        .reactivatedSplitRevision = progress.splitHybrid.revision,
        .activeSplitPageId = progress.splitPageId,
    };
}

} // namespace QindaQt::Test
