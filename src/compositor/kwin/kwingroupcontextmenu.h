// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMenu>
#include <QPointF>
#include <QString>
#include <QVector>

#include <functional>
#include <optional>

class QHideEvent;

namespace QindaQt::Compositor::KWinIntegration {

struct GroupContextMenuDestination final
{
    QString id;
    QString label;
    bool selected = false;

    friend bool operator==(const GroupContextMenuDestination &,
                           const GroupContextMenuDestination &) = default;
};

struct GroupContextMenuState final
{
    bool keepAbove = false;
    bool keepBelow = false;
    bool pinnedToAllWorkspaces = false;
    bool onAllActivities = true;
    QVector<GroupContextMenuDestination> workspaces;
    QVector<GroupContextMenuDestination> activities;
    QVector<GroupContextMenuDestination> outputs;
};

enum class GroupContextMenuCommandKind {
    SetKeepAbove,
    SetKeepBelow,
    SetPinnedToAllWorkspaces,
    ToggleWorkspace,
    SetAllActivities,
    ToggleActivity,
    MoveToOutput,
};

struct GroupContextMenuCommand final
{
    GroupContextMenuCommandKind kind =
        GroupContextMenuCommandKind::SetKeepAbove;
    QString destinationId;
    bool enabled = false;

    friend bool operator==(const GroupContextMenuCommand &,
                           const GroupContextMenuCommand &) = default;
};

// Owns one nonblocking QWidget menu but no KWin objects or container policy.
// Providers and handlers are borrowed by value, run on the GUI thread, and
// must revalidate stable IDs because topology or outputs can change while the
// menu is open.
class KWinGroupContextMenu final : public QMenu
{
public:
    using StateProvider = std::function<std::optional<GroupContextMenuState>(
        const QString &containerId, QString *error)>;
    using CommandHandler = std::function<bool(
        const QString &containerId,
        const GroupContextMenuCommand &command,
        QString *error)>;

    KWinGroupContextMenu(StateProvider stateProvider,
                         CommandHandler commandHandler,
                         QWidget *parent = nullptr);

    [[nodiscard]] bool prepare(const QString &containerId,
                               QString *error = nullptr);
    [[nodiscard]] bool popupForContainer(const QString &containerId,
                                         const QPointF &globalPosition,
                                         QString *error = nullptr);

private:
    struct PendingDispatch final
    {
        QString containerId;
        GroupContextMenuCommand command;
    };

    QAction *addToggleAction(QMenu *menu,
                             const QString &text,
                             const QString &objectName,
                             bool checked,
                             GroupContextMenuCommand command);
    QAction *addDestinationAction(
        QMenu *menu,
        const GroupContextMenuDestination &destination,
        const QString &objectNamePrefix,
        GroupContextMenuCommandKind kind,
        bool checkable);
    void queueDispatch(GroupContextMenuCommand command);
    void schedulePendingDispatches();
    void drainPendingDispatches();
    void dispatch(const QString &containerId,
                  const GroupContextMenuCommand &command);
    void hideEvent(QHideEvent *event) override;

    StateProvider m_stateProvider;
    CommandHandler m_commandHandler;
    QString m_containerId;
    QVector<PendingDispatch> m_pendingDispatches;
    bool m_dispatchScheduled = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
