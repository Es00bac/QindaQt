// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindawindowcontextmenu.h"

#include <QAction>
#include <QCoreApplication>
#include <QHideEvent>
#include <QTimer>

#include <utility>

namespace QindaQt::Decoration {
namespace {

QString menuText(const char *text)
{
    return QCoreApplication::translate("QindaWindowContextMenu", text);
}

} // namespace

QindaWindowContextMenu::QindaWindowContextMenu(CommandHandler handler,
                                               QWidget *parent)
    : QMenu(parent)
    , m_handler(std::move(handler))
{
    setObjectName(QStringLiteral("qindaqt-window-context-menu"));
}

QAction *QindaWindowContextMenu::addCommand(const QString &text,
                                            const QString &objectName,
                                            WindowContextCommand command,
                                            bool enabled, bool checked,
                                            bool checkable)
{
    auto *const action = addAction(text);
    action->setObjectName(objectName);
    action->setEnabled(enabled);
    action->setCheckable(checkable);
    action->setChecked(checkable && checked);
    connect(action, &QAction::triggered, this,
            [this, command] { queueDispatch(command); });
    return action;
}

void QindaWindowContextMenu::prepare(const WindowContextMenuState &state)
{
    clear();
    addCommand(menuText("Minimize"), QStringLiteral("qindaqt-window-minimize"),
               WindowContextCommand::Minimize, state.canMinimize);
    addCommand(state.maximized ? menuText("Restore") : menuText("Maximize"),
               QStringLiteral("qindaqt-window-toggle-maximized"),
               WindowContextCommand::ToggleMaximized, state.canMaximize);
    auto *const shade =
        addCommand(state.shaded ? menuText("Roll Down") : menuText("Roll Up"),
                   QStringLiteral("qindaqt-window-toggle-shaded"),
                   WindowContextCommand::ToggleShaded, state.canShade);
    shade->setVisible(state.canShade);

    addSeparator();
    addCommand(menuText("Show on All Workspaces"),
               QStringLiteral("qindaqt-window-all-workspaces"),
               WindowContextCommand::ToggleAllWorkspaces, true,
               state.onAllWorkspaces, true);
    addCommand(menuText("Always on Top"),
               QStringLiteral("qindaqt-window-keep-above"),
               WindowContextCommand::ToggleKeepAbove, true,
               state.keepAbove, true);
    addCommand(menuText("Always Below"),
               QStringLiteral("qindaqt-window-keep-below"),
               WindowContextCommand::ToggleKeepBelow, true,
               state.keepBelow, true);

    addSeparator();
    addCommand(menuText("Close"), QStringLiteral("qindaqt-window-close"),
               WindowContextCommand::Close, state.canClose);
}

void QindaWindowContextMenu::queueDispatch(WindowContextCommand command)
{
    m_pendingCommands.append(command);
    if (!isVisible()) {
        schedulePendingDispatches();
    }
}

void QindaWindowContextMenu::hideEvent(QHideEvent *event)
{
    QMenu::hideEvent(event);
    schedulePendingDispatches();
}

void QindaWindowContextMenu::schedulePendingDispatches()
{
    if (m_pendingCommands.isEmpty() || m_dispatchScheduled) {
        return;
    }
    m_dispatchScheduled = true;
    QTimer::singleShot(0, this, [this] { drainPendingDispatches(); });
}

void QindaWindowContextMenu::drainPendingDispatches()
{
    m_dispatchScheduled = false;
    if (isVisible()) {
        return;
    }
    const auto commands = std::exchange(m_pendingCommands, {});
    for (const auto command : commands) {
        if (m_handler) {
            m_handler(command);
        }
    }
}

} // namespace QindaQt::Decoration
