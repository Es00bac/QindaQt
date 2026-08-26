// SPDX-License-Identifier: GPL-3.0-or-later
#include "compositordevelopmentworkflow.h"

#include "compositorprobeclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

#include <algorithm>
#include <optional>
#include <utility>

namespace QindaQt::Test {
namespace {

constexpr auto ThirdPageId = "probe-page-third";
constexpr auto ThirdLeafId = "probe-leaf-third";

struct DevelopmentWorkflowState final
{
    QString primaryId;
    QString secondaryId;
    QString pageWindowId;
    QString containerId;
    QString originalPageId;
    QRectF primaryRestoreFrame;
    QRectF secondaryRestoreFrame;
    QRectF pageRestoreFrame;
    QRectF stableOuterFrame;
};

const ObservedWindow &window(const WindowInventory &inventory, const QString &title)
{
    return inventory.constFind(title).value();
}

QByteArray encoded(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QJsonObject transaction(const QString &transactionId, const QString &containerId,
                        const QString &revision, QJsonObject operation)
{
    return {{QStringLiteral("protocol"),
             QJsonObject{{QStringLiteral("major"), 1}, {QStringLiteral("minor"), 0}}},
            {QStringLiteral("transactionId"), transactionId},
            {QStringLiteral("containerId"), containerId},
            {QStringLiteral("expectedRevision"), revision},
            {QStringLiteral("operations"), QJsonArray{std::move(operation)}}};
}

std::optional<QJsonObject> submit(CompositorProbeClient &client, const QString &transactionId,
                                  const QString &containerId, const QString &revision,
                                  QJsonObject operation, QString *error)
{
    return client.call(
        QStringLiteral("Submit"),
        encoded(transaction(transactionId, containerId, revision, std::move(operation))), error);
}

bool rejectedWith(const std::optional<QJsonObject> &reply, QLatin1StringView code)
{
    return reply && reply->value(QStringLiteral("status")) == QStringLiteral("rejected") &&
           reply->value(QStringLiteral("failure"))
                   .toObject()
                   .value(QStringLiteral("code"))
                   .toString() == code;
}

bool expectReply(const std::optional<QJsonObject> &reply, QLatin1StringView status,
                 QLatin1StringView revision, const QString &context, QString *error)
{
    if (reply && reply->value(QStringLiteral("status")).toString() == status &&
        reply->value(QStringLiteral("revision")).toString() == revision) {
        return true;
    }
    if (error->isEmpty()) {
        *error = QStringLiteral("%1; response=%2")
                     .arg(context, QString::fromUtf8(QJsonDocument(reply.value_or(QJsonObject{}))
                                                         .toJson(QJsonDocument::Compact)));
    }
    return false;
}

bool snapshotHasState(const QJsonObject &reply, const QString &activePageId, qsizetype pageCount)
{
    const auto snapshot = reply.value(QStringLiteral("snapshot")).toObject();
    return snapshot.value(QStringLiteral("activePageId")).toString() == activePageId &&
           snapshot.value(QStringLiteral("pages")).toArray().size() == pageCount;
}

bool tiledInFrame(const ObservedWindow &primary, const ObservedWindow &secondary,
                  const QRectF &outerFrame)
{
    const bool adjacent =
        primary.frame.isValid() && secondary.frame.isValid() &&
        nearlyEqual(primary.frame.x() + primary.frame.width(), secondary.frame.x()) &&
        nearlyEqual(primary.frame.y(), secondary.frame.y()) &&
        nearlyEqual(primary.frame.height(), secondary.frame.height());
    return adjacent && sameGeometry(primary.frame.united(secondary.frame), outerFrame);
}

bool independentlyRestored(const ObservedWindow &observed, const QRectF &restoreFrame)
{
    return observed.containerId.isEmpty() && !observed.minimized &&
           sameGeometry(observed.frame, restoreFrame);
}

bool onlyContainer(const QJsonArray &containers, const QString &containerId,
                   QLatin1StringView revision)
{
    if (containers.size() != 1) {
        return false;
    }
    const auto container = containers.at(0).toObject();
    return container.value(QStringLiteral("id")).toString() == containerId &&
           container.value(QStringLiteral("revision")).toString() == revision;
}

bool redockAndRelease(CompositorProbeClient &client, const ProbeWindowTitles &titles,
                      const QString &primaryId, const QString &secondaryId,
                      const QRectF &primaryRestoreFrame, const QRectF &secondaryRestoreFrame,
                      const QRectF &pageRestoreFrame, QString *releasedContainerId, QString *error)
{
    const auto redocked = client.dock(primaryId, secondaryId, 0.5, error);
    if (!expectReply(redocked, QLatin1StringView("docked"), QLatin1StringView("1"),
                     QStringLiteral("second atomic docking failed"), error)) {
        return false;
    }
    const auto containerId = redocked->value(QStringLiteral("containerId")).toString();
    const auto grouped = client.awaitWindows(
        {titles.primary, titles.secondary, titles.page},
        [&](const WindowInventory &inventory) {
            const auto &primary = window(inventory, titles.primary);
            const auto &secondary = window(inventory, titles.secondary);
            const auto &page = window(inventory, titles.page);
            const auto frame = primary.frame.united(secondary.frame);
            return primary.containerId == containerId && secondary.containerId == containerId &&
                   !primary.minimized && !secondary.minimized &&
                   tiledInFrame(primary, secondary, frame) &&
                   independentlyRestored(page, pageRestoreFrame);
        },
        error);
    if (!grouped) {
        *error = QStringLiteral("second split realization: %1").arg(*error);
        return false;
    }

    const auto released = client.call(QStringLiteral("ReleaseContainer"), containerId, error);
    if (!released || released->value(QStringLiteral("status")) != QStringLiteral("released")) {
        if (error->isEmpty()) {
            *error = QStringLiteral("container release did not commit");
        }
        return false;
    }
    const auto restored = client.awaitWindows(
        {titles.primary, titles.secondary, titles.page},
        [&](const WindowInventory &inventory) {
            return independentlyRestored(window(inventory, titles.primary), primaryRestoreFrame) &&
                   independentlyRestored(window(inventory, titles.secondary),
                                         secondaryRestoreFrame) &&
                   independentlyRestored(window(inventory, titles.page), pageRestoreFrame);
        },
        error);
    const auto containersAfter = client.containers(error);
    if (!restored || !containersAfter || !containersAfter->isEmpty()) {
        if (error->isEmpty()) {
            *error = QStringLiteral("container release did not restore all independent windows");
        }
        return false;
    }
    *releasedContainerId = containerId;
    return true;
}

std::optional<DevelopmentWorkflowState> establishInitialContainer(CompositorProbeClient &client,
                                                                  const ProbeWindowTitles &titles,
                                                                  QString *error)
{
    const auto initial = client.awaitWindows(
        {titles.primary, titles.secondary, titles.page},
        [](const WindowInventory &inventory) {
            return std::all_of(inventory.cbegin(), inventory.cend(), [](const auto &observed) {
                return observed.containerId.isEmpty() && !observed.minimized &&
                       observed.frame.isValid();
            });
        },
        error);
    if (!initial) {
        *error = QStringLiteral("the compositor did not expose three independent probes: %1")
                     .arg(*error);
        return std::nullopt;
    }

    DevelopmentWorkflowState state;
    state.primaryId = window(*initial, titles.primary).id;
    state.secondaryId = window(*initial, titles.secondary).id;
    state.pageWindowId = window(*initial, titles.page).id;
    state.primaryRestoreFrame = window(*initial, titles.primary).frame;
    state.secondaryRestoreFrame = window(*initial, titles.secondary).frame;
    state.pageRestoreFrame = window(*initial, titles.page).frame;

    const auto invalidDock = client.dock(state.primaryId, state.secondaryId, 1.0, error);
    const auto afterInvalid = client.containers(error);
    if (!rejectedWith(invalidDock, QLatin1StringView("malformed-dock-request")) || !afterInvalid ||
        !afterInvalid->isEmpty()) {
        if (error->isEmpty()) {
            *error = QStringLiteral("invalid docking did not roll back cleanly");
        }
        return std::nullopt;
    }

    const auto dockReply = client.dock(state.primaryId, state.secondaryId, 0.5, error);
    if (!expectReply(dockReply, QLatin1StringView("docked"), QLatin1StringView("1"),
                     QStringLiteral("atomic docking did not commit"), error)) {
        return std::nullopt;
    }
    state.containerId = dockReply->value(QStringLiteral("containerId")).toString();
    state.originalPageId = dockReply->value(QStringLiteral("snapshot"))
                               .toObject()
                               .value(QStringLiteral("activePageId"))
                               .toString();
    if (state.containerId.isEmpty() || state.originalPageId.isEmpty() ||
        !snapshotHasState(*dockReply, state.originalPageId, 1)) {
        *error = QStringLiteral("atomic docking returned an invalid container snapshot");
        return std::nullopt;
    }

    const auto grouped = client.awaitWindows(
        {titles.primary, titles.secondary, titles.page},
        [&](const WindowInventory &inventory) {
            const auto &primary = window(inventory, titles.primary);
            const auto &secondary = window(inventory, titles.secondary);
            const auto outerFrame = primary.frame.united(secondary.frame);
            return primary.containerId == state.containerId &&
                   secondary.containerId == state.containerId && !primary.minimized &&
                   !secondary.minimized && tiledInFrame(primary, secondary, outerFrame) &&
                   independentlyRestored(window(inventory, titles.page), state.pageRestoreFrame);
        },
        error);
    if (!grouped) {
        *error = QStringLiteral("initial split realization: %1").arg(*error);
        return std::nullopt;
    }
    state.stableOuterFrame =
        window(*grouped, titles.primary).frame.united(window(*grouped, titles.secondary).frame);

    const auto ownedDock = client.dock(state.primaryId, state.secondaryId, 0.5, error);
    const auto afterOwned = client.containers(error);
    if (!rejectedWith(ownedDock, QLatin1StringView("already-owned")) || !afterOwned ||
        !onlyContainer(*afterOwned, state.containerId, QLatin1StringView("1"))) {
        if (error->isEmpty()) {
            *error = QStringLiteral("owned-window docking did not preserve the existing group");
        }
        return std::nullopt;
    }
    return state;
}

bool exercisePageLifecycle(CompositorProbeClient &client, const ProbeWindowTitles &titles,
                           const DevelopmentWorkflowState &state, QString *error)
{
    const QJsonObject addPage{{QStringLiteral("type"), QStringLiteral("add-page")},
                              {QStringLiteral("pageId"), QString::fromLatin1(ThirdPageId)},
                              {QStringLiteral("leafNodeId"), QString::fromLatin1(ThirdLeafId)},
                              {QStringLiteral("windowId"), state.pageWindowId}};
    const auto added = submit(client, QStringLiteral("probe-add-page"), state.containerId,
                              QStringLiteral("1"), addPage, error);
    if (!expectReply(added, QLatin1StringView("committed"), QLatin1StringView("2"),
                     QStringLiteral("third-page transaction did not commit"), error) ||
        !snapshotHasState(*added, state.originalPageId, 2)) {
        if (error->isEmpty()) {
            *error = QStringLiteral("third-page snapshot was not valid");
        }
        return false;
    }
    const auto inactivePage = client.awaitWindows(
        {titles.primary, titles.secondary, titles.page},
        [&](const WindowInventory &inventory) {
            const auto &primary = window(inventory, titles.primary);
            const auto &secondary = window(inventory, titles.secondary);
            const auto &page = window(inventory, titles.page);
            return primary.containerId == state.containerId &&
                   secondary.containerId == state.containerId &&
                   page.containerId == state.containerId && !primary.minimized &&
                   !secondary.minimized && page.minimized &&
                   tiledInFrame(primary, secondary, state.stableOuterFrame) &&
                   sameGeometry(page.targetFrame, state.stableOuterFrame);
        },
        error);
    if (!inactivePage) {
        *error = QStringLiteral("inactive page ownership/target-frame realization: %1").arg(*error);
        return false;
    }

    const QJsonObject activateThird{{QStringLiteral("type"), QStringLiteral("activate-page")},
                                    {QStringLiteral("pageId"), QString::fromLatin1(ThirdPageId)}};
    const auto activatedThird =
        submit(client, QStringLiteral("probe-activate-third"), state.containerId,
               QStringLiteral("2"), activateThird, error);
    if (!expectReply(activatedThird, QLatin1StringView("committed"), QLatin1StringView("3"),
                     QStringLiteral("third-page activation did not commit"), error) ||
        !snapshotHasState(*activatedThird, QString::fromLatin1(ThirdPageId), 2)) {
        if (error->isEmpty()) {
            *error = QStringLiteral("third-page activation snapshot was not valid");
        }
        return false;
    }
    const auto thirdVisible = client.awaitWindows(
        {titles.primary, titles.secondary, titles.page},
        [&](const WindowInventory &inventory) {
            const auto &primary = window(inventory, titles.primary);
            const auto &secondary = window(inventory, titles.secondary);
            const auto &page = window(inventory, titles.page);
            return primary.containerId == state.containerId &&
                   secondary.containerId == state.containerId &&
                   page.containerId == state.containerId && primary.minimized &&
                   secondary.minimized && !page.minimized &&
                   sameGeometry(page.frame, state.stableOuterFrame);
        },
        error);
    if (!thirdVisible) {
        *error =
            QStringLiteral("active third page did not isolate in the stable frame: %1").arg(*error);
        return false;
    }

    const QJsonObject activateOriginal{{QStringLiteral("type"), QStringLiteral("activate-page")},
                                       {QStringLiteral("pageId"), state.originalPageId}};
    const auto activatedOriginal =
        submit(client, QStringLiteral("probe-reactivate-original"), state.containerId,
               QStringLiteral("3"), activateOriginal, error);
    if (!expectReply(activatedOriginal, QLatin1StringView("committed"), QLatin1StringView("4"),
                     QStringLiteral("original-page reactivation did not commit"), error) ||
        !snapshotHasState(*activatedOriginal, state.originalPageId, 2)) {
        if (error->isEmpty()) {
            *error = QStringLiteral("original-page activation snapshot was not valid");
        }
        return false;
    }
    const auto originalVisible = client.awaitWindows(
        {titles.primary, titles.secondary, titles.page},
        [&](const WindowInventory &inventory) {
            const auto &primary = window(inventory, titles.primary);
            const auto &secondary = window(inventory, titles.secondary);
            const auto &page = window(inventory, titles.page);
            return !primary.minimized && !secondary.minimized && page.minimized &&
                   tiledInFrame(primary, secondary, state.stableOuterFrame) &&
                   sameGeometry(page.targetFrame, state.stableOuterFrame);
        },
        error);
    if (!originalVisible) {
        *error =
            QStringLiteral("original page did not reactivate in the stable frame: %1").arg(*error);
        return false;
    }

    const QJsonObject detachPage{{QStringLiteral("type"), QStringLiteral("detach-window")},
                                 {QStringLiteral("windowId"), state.pageWindowId}};
    const auto detachedPage = submit(client, QStringLiteral("probe-detach-third"),
                                     state.containerId, QStringLiteral("4"), detachPage, error);
    if (!expectReply(detachedPage, QLatin1StringView("committed"), QLatin1StringView("5"),
                     QStringLiteral("third-page detach did not commit"), error) ||
        !snapshotHasState(*detachedPage, state.originalPageId, 1)) {
        if (error->isEmpty()) {
            *error = QStringLiteral("third-page detach snapshot was not valid");
        }
        return false;
    }
    const auto pageDetached = client.awaitWindows(
        {titles.primary, titles.secondary, titles.page},
        [&](const WindowInventory &inventory) {
            const auto &primary = window(inventory, titles.primary);
            const auto &secondary = window(inventory, titles.secondary);
            return primary.containerId == state.containerId &&
                   secondary.containerId == state.containerId && !primary.minimized &&
                   !secondary.minimized &&
                   tiledInFrame(primary, secondary, state.stableOuterFrame) &&
                   independentlyRestored(window(inventory, titles.page), state.pageRestoreFrame);
        },
        error);
    if (!pageDetached) {
        *error = QStringLiteral("third-page restoration: %1").arg(*error);
        return false;
    }
    return true;
}

bool exerciseSingletonUnwrap(CompositorProbeClient &client, const ProbeWindowTitles &titles,
                             const DevelopmentWorkflowState &state, QString *error)
{
    const QJsonObject detachSecondary{{QStringLiteral("type"), QStringLiteral("detach-window")},
                                      {QStringLiteral("windowId"), state.secondaryId}};
    const auto unwrapped = submit(client, QStringLiteral("probe-detach-secondary"),
                                  state.containerId, QStringLiteral("5"), detachSecondary, error);
    if (!expectReply(unwrapped, QLatin1StringView("committed"), QLatin1StringView("6"),
                     QStringLiteral("singleton unwrapping did not commit"), error) ||
        !snapshotHasState(*unwrapped, QString{}, 0)) {
        if (error->isEmpty()) {
            *error = QStringLiteral("singleton unwrapping snapshot was not empty");
        }
        return false;
    }
    const auto allDetached = client.awaitWindows(
        {titles.primary, titles.secondary, titles.page},
        [&](const WindowInventory &inventory) {
            return independentlyRestored(window(inventory, titles.primary),
                                         state.primaryRestoreFrame) &&
                   independentlyRestored(window(inventory, titles.secondary),
                                         state.secondaryRestoreFrame) &&
                   independentlyRestored(window(inventory, titles.page), state.pageRestoreFrame);
        },
        error);
    QString containerError;
    const auto afterUnwrap = client.containers(&containerError);
    if (!allDetached || !afterUnwrap || !afterUnwrap->isEmpty()) {
        if (error->isEmpty()) {
            *error = containerError.isEmpty()
                         ? QStringLiteral("singleton unwrapping did not restore independent state")
                         : containerError;
        }
        return false;
    }
    return true;
}

} // namespace

std::optional<QJsonObject> exerciseDevelopmentWorkflow(CompositorProbeClient &client,
                                                       const ProbeWindowTitles &titles,
                                                       QString *error)
{
    const auto state = establishInitialContainer(client, titles, error);
    if (!state || !exercisePageLifecycle(client, titles, *state, error) ||
        !exerciseSingletonUnwrap(client, titles, *state, error)) {
        return std::nullopt;
    }

    QString releasedContainerId;
    if (!redockAndRelease(client, titles, state->primaryId, state->secondaryId,
                          state->primaryRestoreFrame, state->secondaryRestoreFrame,
                          state->pageRestoreFrame, &releasedContainerId, error)) {
        return std::nullopt;
    }

    return QJsonObject{{QStringLiteral("containerId"), state->containerId},
                       {QStringLiteral("releasedContainerId"), releasedContainerId},
                       {QStringLiteral("primaryWindowId"), state->primaryId},
                       {QStringLiteral("secondaryWindowId"), state->secondaryId},
                       {QStringLiteral("pageWindowId"), state->pageWindowId},
                       {QStringLiteral("originalPageId"), state->originalPageId},
                       {QStringLiteral("thirdPageId"), QString::fromLatin1(ThirdPageId)},
                       {QStringLiteral("splitRevision"), QStringLiteral("1")},
                       {QStringLiteral("addPageRevision"), QStringLiteral("2")},
                       {QStringLiteral("activateThirdRevision"), QStringLiteral("3")},
                       {QStringLiteral("reactivateOriginalRevision"), QStringLiteral("4")},
                       {QStringLiteral("detachThirdRevision"), QStringLiteral("5")},
                       {QStringLiteral("singletonUnwrapRevision"), QStringLiteral("6")},
                       {QStringLiteral("inactivePageTargetAnchoredAndMinimized"), true},
                       {QStringLiteral("activePageIsolation"), true},
                       {QStringLiteral("stableOuterFrame"), true},
                       {QStringLiteral("restoredThirdGeometry"), true},
                       {QStringLiteral("automaticSingletonUnwrap"), true},
                       {QStringLiteral("invalidDockRollback"), true},
                       {QStringLiteral("ownedDockRollback"), true},
                       {QStringLiteral("released"), true}};
}

} // namespace QindaQt::Test
