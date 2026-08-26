// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwingroupcontextmenu.h"

#include <QAction>
#include <QActionGroup>
#include <QCoreApplication>
#include <QHideEvent>
#include <QSet>
#include <QTimer>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

QString menuText(const char *text)
{
    return QCoreApplication::translate("KWinGroupContextMenu", text);
}

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

bool destinationsValid(
    const QVector<GroupContextMenuDestination> &destinations,
    QLatin1StringView kind,
    QString *error)
{
    QSet<QString> ids;
    for (const auto &destination : destinations) {
        if (destination.id.isEmpty() || destination.label.isEmpty()
            || ids.contains(destination.id)) {
            return fail(error,
                        QStringLiteral("group context menu has an invalid %1 destination")
                            .arg(kind));
        }
        ids.insert(destination.id);
    }
    return true;
}

} // namespace

KWinGroupContextMenu::KWinGroupContextMenu(
    StateProvider stateProvider,
    CommandHandler commandHandler,
    QWidget *parent)
    : QMenu(parent)
    , m_stateProvider(std::move(stateProvider))
    , m_commandHandler(std::move(commandHandler))
{
    setObjectName(QStringLiteral("qindaqt-group-context-menu"));
}

QAction *KWinGroupContextMenu::addToggleAction(
    QMenu *menu,
    const QString &text,
    const QString &objectName,
    bool checked,
    GroupContextMenuCommand command)
{
    auto *const action = menu->addAction(text);
    action->setObjectName(objectName);
    action->setCheckable(true);
    action->setChecked(checked);
    connect(action, &QAction::triggered, this,
            [this, command = std::move(command)](bool enabled) mutable {
                command.enabled = enabled;
                queueDispatch(command);
            });
    return action;
}

QAction *KWinGroupContextMenu::addDestinationAction(
    QMenu *menu,
    const GroupContextMenuDestination &destination,
    const QString &objectNamePrefix,
    GroupContextMenuCommandKind kind,
    bool checkable)
{
    auto *const action = menu->addAction(destination.label);
    action->setObjectName(objectNamePrefix + destination.id);
    action->setCheckable(checkable);
    action->setChecked(checkable && destination.selected);
    connect(action, &QAction::triggered, this,
            [this, command = GroupContextMenuCommand{
                       .kind = kind,
                       .destinationId = destination.id,
                       .enabled = true,
                   }, checkable](bool checked) mutable {
                command.enabled = checkable ? checked : true;
                queueDispatch(command);
            });
    return action;
}

bool KWinGroupContextMenu::prepare(const QString &containerId, QString *error)
{
    if (error) {
        error->clear();
    }
    if (containerId.isEmpty() || !m_stateProvider || !m_commandHandler) {
        return fail(error,
                    QStringLiteral("group context menu dependencies are incomplete"));
    }
    auto state = m_stateProvider(containerId, error);
    if (!state) {
        return false;
    }
    if (state->keepAbove && state->keepBelow) {
        return fail(error,
                    QStringLiteral("group context menu received contradictory layers"));
    }
    if (!destinationsValid(state->workspaces,
                           QLatin1StringView("workspace"), error)
        || !destinationsValid(state->activities,
                              QLatin1StringView("activity"), error)
        || !destinationsValid(state->outputs,
                              QLatin1StringView("output"), error)) {
        return false;
    }

    clear();
    m_containerId = containerId;
    auto *const keepAbove = addToggleAction(
        this, menuText("Keep Above"),
        QStringLiteral("qindaqt-context-keep-above"), state->keepAbove,
        {.kind = GroupContextMenuCommandKind::SetKeepAbove,
         .destinationId = {},
         .enabled = false});
    // Parent the ephemeral group to an action removed by QMenu::clear(); the
    // menu is rebuilt for every request and must not accumulate helpers.
    auto *const layerGroup = new QActionGroup(keepAbove);
    layerGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    layerGroup->addAction(keepAbove);
    layerGroup->addAction(addToggleAction(
        this, menuText("Keep Below"),
        QStringLiteral("qindaqt-context-keep-below"), state->keepBelow,
        {.kind = GroupContextMenuCommandKind::SetKeepBelow,
         .destinationId = {},
         .enabled = false}));
    addSeparator();
    addToggleAction(this, menuText("Pin to All Workspaces"),
                    QStringLiteral("qindaqt-context-pin-workspaces"),
                    state->pinnedToAllWorkspaces,
                    {.kind = GroupContextMenuCommandKind::SetPinnedToAllWorkspaces,
                     .destinationId = {},
                     .enabled = false});

    auto *const workspaceMenu = addMenu(menuText("Workspaces"));
    workspaceMenu->setObjectName(QStringLiteral("qindaqt-context-workspaces"));
    for (const auto &destination : std::as_const(state->workspaces)) {
        addDestinationAction(workspaceMenu, destination,
                             QStringLiteral("qindaqt-context-workspace-"),
                             GroupContextMenuCommandKind::ToggleWorkspace,
                             true);
    }
    workspaceMenu->setEnabled(!state->workspaces.isEmpty());

    auto *const activityMenu = addMenu(menuText("Activities"));
    activityMenu->setObjectName(QStringLiteral("qindaqt-context-activities"));
    addToggleAction(activityMenu, menuText("All Activities"),
                    QStringLiteral("qindaqt-context-all-activities"),
                    state->onAllActivities,
                    {.kind = GroupContextMenuCommandKind::SetAllActivities,
                     .destinationId = {},
                     .enabled = false});
    activityMenu->addSeparator();
    for (const auto &destination : std::as_const(state->activities)) {
        addDestinationAction(activityMenu, destination,
                             QStringLiteral("qindaqt-context-activity-"),
                             GroupContextMenuCommandKind::ToggleActivity,
                             true);
    }
    activityMenu->setEnabled(!state->activities.isEmpty());

    auto *const outputMenu = addMenu(menuText("Move to Output"));
    outputMenu->setObjectName(QStringLiteral("qindaqt-context-outputs"));
    auto *const outputGroup = new QActionGroup(outputMenu);
    for (const auto &destination : std::as_const(state->outputs)) {
        outputGroup->addAction(addDestinationAction(
            outputMenu, destination,
            QStringLiteral("qindaqt-context-output-"),
            GroupContextMenuCommandKind::MoveToOutput, true));
    }
    outputMenu->setEnabled(!state->outputs.isEmpty());
    return true;
}

bool KWinGroupContextMenu::popupForContainer(
    const QString &containerId,
    const QPointF &globalPosition,
    QString *error)
{
    if (!prepare(containerId, error)) {
        return false;
    }
    popup(globalPosition.toPoint());
    return true;
}

void KWinGroupContextMenu::queueDispatch(GroupContextMenuCommand command)
{
    m_pendingDispatches.append({m_containerId, std::move(command)});
    // AGENT-GUARD: QAction::triggered is emitted before QMenu finishes
    // dismissing its popup. Mutating KWin's layer or output inline lets that
    // transient popup split the group's stacking block and hides shared
    // chrome. Capture the stable target now; hideEvent schedules delivery only
    // after the top-level popup has actually completed its hide lifecycle.
    if (!isVisible()) {
        schedulePendingDispatches();
    }
}

void KWinGroupContextMenu::hideEvent(QHideEvent *event)
{
    QMenu::hideEvent(event);
    schedulePendingDispatches();
}

void KWinGroupContextMenu::schedulePendingDispatches()
{
    if (m_pendingDispatches.isEmpty() || m_dispatchScheduled) {
        return;
    }
    m_dispatchScheduled = true;
    QTimer::singleShot(0, this, [this] { drainPendingDispatches(); });
}

void KWinGroupContextMenu::drainPendingDispatches()
{
    m_dispatchScheduled = false;
    if (isVisible()) {
        // A popup reopened before the queued delivery. Keep the captured
        // command pending; the next completed hide schedules another attempt.
        return;
    }
    const auto pending = std::exchange(m_pendingDispatches, {});
    for (const auto &request : pending) {
        dispatch(request.containerId, request.command);
    }
}

void KWinGroupContextMenu::dispatch(
    const QString &containerId,
    const GroupContextMenuCommand &command)
{
    QString error;
    if (!m_commandHandler
        || !m_commandHandler(containerId, command, &error)) {
        qWarning("QindaQt group context command failed: %s",
                 qPrintable(error.isEmpty()
                                ? QStringLiteral("command handler is unavailable")
                                : error));
    }
}

} // namespace QindaQt::Compositor::KWinIntegration
