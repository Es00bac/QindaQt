// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMenu>
#include <QVector>

#include <functional>

class QAction;
class QHideEvent;

namespace QindaQt::Decoration {

enum class WindowContextCommand {
    Minimize,
    ToggleMaximized,
    ToggleShaded,
    ToggleAllWorkspaces,
    ToggleKeepAbove,
    ToggleKeepBelow,
    Close,
};

struct WindowContextMenuState final
{
    bool canMinimize = false;
    bool canMaximize = false;
    bool maximized = false;
    bool canShade = false;
    bool shaded = false;
    bool onAllWorkspaces = false;
    bool keepAbove = false;
    bool keepBelow = false;
    bool canClose = false;
};

// AGENT-GUARD: This ordinary-window menu uses only public KDecoration
// commands, and dispatches after hiding so close/minimize cannot destroy the
// decoration while QMenu is still delivering its triggered signal.
class QindaWindowContextMenu final : public QMenu
{
public:
    using CommandHandler = std::function<void(WindowContextCommand)>;

    explicit QindaWindowContextMenu(CommandHandler handler,
                                    QWidget *parent = nullptr);

    void prepare(const WindowContextMenuState &state);

private:
    QAction *addCommand(const QString &text, const QString &objectName,
                        WindowContextCommand command, bool enabled = true,
                        bool checked = false, bool checkable = false);
    void queueDispatch(WindowContextCommand command);
    void schedulePendingDispatches();
    void drainPendingDispatches();
    void hideEvent(QHideEvent *event) override;

    CommandHandler m_handler;
    QVector<WindowContextCommand> m_pendingCommands;
    bool m_dispatchScheduled = false;
};

} // namespace QindaQt::Decoration
