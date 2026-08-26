// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridshortcutmanager.h"

#include <KGlobalAccel>

#include <QAction>
#include <QKeySequence>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

std::unique_ptr<QAction> makeAction(QString objectName,
                                    QString text,
                                    const std::function<void()> &trigger)
{
    auto action = std::make_unique<QAction>();
    action->setObjectName(std::move(objectName));
    action->setText(std::move(text));
    QObject::connect(action.get(), &QAction::triggered, [trigger] {
        if (trigger) {
            trigger();
        }
    });
    return action;
}

bool registerAction(QAction *action, const QKeySequence &shortcut)
{
    const QList<QKeySequence> shortcuts{shortcut};
    auto *const globalAccel = KGlobalAccel::self();
    const bool defaultRegistered = globalAccel->setDefaultShortcut(
        action, shortcuts, KGlobalAccel::Autoloading);
    // AGENT-CONTRACT: Autoloading preserves a user's reassigned or disabled
    // shortcut. NoAutoloading would reclaim defaults at every plugin load.
    const bool activeRegistered = globalAccel->setShortcut(
        action, shortcuts, KGlobalAccel::Autoloading);
    return defaultRegistered && activeRegistered;
}

const std::function<void()> &triggerFor(const HybridShortcutTriggers &triggers,
                                        HybridShortcutAction action)
{
    switch (action) {
    case HybridShortcutAction::Dock:
        return triggers.dock;
    case HybridShortcutAction::DockPage:
        return triggers.dockPage;
    case HybridShortcutAction::MoveGroup:
        return triggers.moveGroup;
    case HybridShortcutAction::ResizeActiveSplit:
        return triggers.resizeActiveSplit;
    case HybridShortcutAction::ResizeGroup:
        return triggers.resizeGroup;
    case HybridShortcutAction::NextPage:
        return triggers.nextPage;
    case HybridShortcutAction::PreviousPage:
        return triggers.previousPage;
    case HybridShortcutAction::ReorderPageNext:
        return triggers.reorderPageNext;
    case HybridShortcutAction::ReorderPagePrevious:
        return triggers.reorderPagePrevious;
    case HybridShortcutAction::CloseGroup:
        return triggers.closeGroup;
    case HybridShortcutAction::MinimizeGroup:
        return triggers.minimizeGroup;
    case HybridShortcutAction::MaximizeGroup:
        return triggers.maximizeGroup;
    case HybridShortcutAction::RestoreGroup:
        return triggers.restoreGroup;
    case HybridShortcutAction::Count:
        break;
    }
    static const std::function<void()> empty;
    return empty;
}

QString actionText(HybridShortcutAction action)
{
    switch (action) {
    case HybridShortcutAction::Dock:
        return QStringLiteral("Start QindaQt window docking");
    case HybridShortcutAction::DockPage:
        return QStringLiteral("Start QindaQt page docking");
    case HybridShortcutAction::MoveGroup:
        return QStringLiteral("Move the active QindaQt window group");
    case HybridShortcutAction::ResizeActiveSplit:
        return QStringLiteral("Resize the active QindaQt split");
    case HybridShortcutAction::ResizeGroup:
        return QStringLiteral("Resize the active QindaQt window group");
    case HybridShortcutAction::NextPage:
        return QStringLiteral("Activate the next QindaQt group page");
    case HybridShortcutAction::PreviousPage:
        return QStringLiteral("Activate the previous QindaQt group page");
    case HybridShortcutAction::ReorderPageNext:
        return QStringLiteral("Move the active QindaQt page forward");
    case HybridShortcutAction::ReorderPagePrevious:
        return QStringLiteral("Move the active QindaQt page backward");
    case HybridShortcutAction::CloseGroup:
        return QStringLiteral("Close the active QindaQt window group");
    case HybridShortcutAction::MinimizeGroup:
        return QStringLiteral("Minimize the active QindaQt window group");
    case HybridShortcutAction::MaximizeGroup:
        return QStringLiteral("Maximize the active QindaQt window group");
    case HybridShortcutAction::RestoreGroup:
        return QStringLiteral("Restore the active QindaQt window group");
    case HybridShortcutAction::Count:
        return {};
    }
    return {};
}

} // namespace

HybridShortcutManager::HybridShortcutManager(HybridShortcutTriggers triggers,
                                             bool registerGlobally)
    : m_triggers(std::move(triggers))
{
    for (std::size_t index = 0; index < ActionCount; ++index) {
        const auto kind = static_cast<HybridShortcutAction>(index);
        m_actions[index] = makeAction(stableActionId(kind), actionText(kind),
                                      triggerFor(m_triggers, kind));
    }
    if (!registerGlobally) {
        return;
    }

    // Do not short-circuit registration: a D-Bus failure for one action must
    // not prevent the remaining stable IDs from being user-remappable.
    m_registered = true;
    for (std::size_t index = 0; index < ActionCount; ++index) {
        const auto kind = static_cast<HybridShortcutAction>(index);
        m_registered = registerAction(m_actions[index].get(), defaultShortcut(kind))
            && m_registered;
    }
}

HybridShortcutManager::~HybridShortcutManager() = default;

QKeySequence HybridShortcutManager::defaultShortcut(HybridShortcutAction action)
{
    switch (action) {
    case HybridShortcutAction::Dock:
        return QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_D);
    case HybridShortcutAction::DockPage:
        return QKeySequence(Qt::META | Qt::CTRL | Qt::SHIFT | Qt::Key_D);
    case HybridShortcutAction::MoveGroup:
        return QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_M);
    case HybridShortcutAction::ResizeActiveSplit:
        return QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_S);
    case HybridShortcutAction::ResizeGroup:
        return QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_R);
    case HybridShortcutAction::NextPage:
        return QKeySequence(Qt::META | Qt::CTRL | Qt::Key_PageDown);
    case HybridShortcutAction::PreviousPage:
        return QKeySequence(Qt::META | Qt::CTRL | Qt::Key_PageUp);
    case HybridShortcutAction::ReorderPageNext:
        return QKeySequence(Qt::META | Qt::CTRL | Qt::SHIFT | Qt::Key_PageDown);
    case HybridShortcutAction::ReorderPagePrevious:
        return QKeySequence(Qt::META | Qt::CTRL | Qt::SHIFT | Qt::Key_PageUp);
    case HybridShortcutAction::CloseGroup:
        return QKeySequence(Qt::META | Qt::CTRL | Qt::SHIFT | Qt::Key_Q);
    case HybridShortcutAction::MinimizeGroup:
        return QKeySequence(Qt::META | Qt::CTRL | Qt::SHIFT | Qt::Key_N);
    case HybridShortcutAction::MaximizeGroup:
        return QKeySequence(Qt::META | Qt::CTRL | Qt::SHIFT | Qt::Key_X);
    case HybridShortcutAction::RestoreGroup:
        return QKeySequence(Qt::META | Qt::CTRL | Qt::SHIFT | Qt::Key_U);
    case HybridShortcutAction::Count:
        return {};
    }
    return {};
}

QString HybridShortcutManager::stableActionId(HybridShortcutAction action)
{
    switch (action) {
    case HybridShortcutAction::Dock:
        return QStringLiteral("qindaqt_keyboard_dock");
    case HybridShortcutAction::DockPage:
        return QStringLiteral("qindaqt_keyboard_dock_page");
    case HybridShortcutAction::MoveGroup:
        return QStringLiteral("qindaqt_keyboard_move_group");
    case HybridShortcutAction::ResizeActiveSplit:
        return QStringLiteral("qindaqt_keyboard_resize_active_split");
    case HybridShortcutAction::ResizeGroup:
        return QStringLiteral("qindaqt_keyboard_resize_group");
    case HybridShortcutAction::NextPage:
        return QStringLiteral("qindaqt_keyboard_next_page");
    case HybridShortcutAction::PreviousPage:
        return QStringLiteral("qindaqt_keyboard_previous_page");
    case HybridShortcutAction::ReorderPageNext:
        return QStringLiteral("qindaqt_keyboard_reorder_page_next");
    case HybridShortcutAction::ReorderPagePrevious:
        return QStringLiteral("qindaqt_keyboard_reorder_page_previous");
    case HybridShortcutAction::CloseGroup:
        return QStringLiteral("qindaqt_keyboard_close_group");
    case HybridShortcutAction::MinimizeGroup:
        return QStringLiteral("qindaqt_keyboard_minimize_group");
    case HybridShortcutAction::MaximizeGroup:
        return QStringLiteral("qindaqt_keyboard_maximize_group");
    case HybridShortcutAction::RestoreGroup:
        return QStringLiteral("qindaqt_keyboard_restore_group");
    case HybridShortcutAction::Count:
        return {};
    }
    return {};
}

QAction *HybridShortcutManager::action(HybridShortcutAction kind) const noexcept
{
    const auto index = static_cast<std::size_t>(kind);
    return index < ActionCount ? m_actions[index].get() : nullptr;
}

QAction *HybridShortcutManager::keyboardDockAction() const noexcept
{
    return action(HybridShortcutAction::Dock);
}

QAction *HybridShortcutManager::keyboardDockPageAction() const noexcept
{
    return action(HybridShortcutAction::DockPage);
}

QAction *HybridShortcutManager::keyboardMoveGroupAction() const noexcept
{
    return action(HybridShortcutAction::MoveGroup);
}

QAction *HybridShortcutManager::keyboardResizeActiveSplitAction() const noexcept
{
    return action(HybridShortcutAction::ResizeActiveSplit);
}

QAction *HybridShortcutManager::keyboardResizeGroupAction() const noexcept
{
    return action(HybridShortcutAction::ResizeGroup);
}

QAction *HybridShortcutManager::keyboardNextPageAction() const noexcept
{
    return action(HybridShortcutAction::NextPage);
}

QAction *HybridShortcutManager::keyboardPreviousPageAction() const noexcept
{
    return action(HybridShortcutAction::PreviousPage);
}

QAction *HybridShortcutManager::keyboardReorderPageNextAction() const noexcept
{
    return action(HybridShortcutAction::ReorderPageNext);
}

QAction *HybridShortcutManager::keyboardReorderPagePreviousAction() const noexcept
{
    return action(HybridShortcutAction::ReorderPagePrevious);
}

QAction *HybridShortcutManager::keyboardCloseGroupAction() const noexcept
{
    return action(HybridShortcutAction::CloseGroup);
}

QAction *HybridShortcutManager::keyboardMinimizeGroupAction() const noexcept
{
    return action(HybridShortcutAction::MinimizeGroup);
}

QAction *HybridShortcutManager::keyboardMaximizeGroupAction() const noexcept
{
    return action(HybridShortcutAction::MaximizeGroup);
}

QAction *HybridShortcutManager::keyboardRestoreGroupAction() const noexcept
{
    return action(HybridShortcutAction::RestoreGroup);
}

} // namespace QindaQt::Compositor::KWinIntegration
