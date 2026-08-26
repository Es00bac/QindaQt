// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QKeySequence>

#include <array>
#include <functional>
#include <memory>

class QAction;

namespace QindaQt::Compositor::KWinIntegration {

enum class HybridShortcutAction {
    Dock,
    DockPage,
    MoveGroup,
    ResizeActiveSplit,
    ResizeGroup,
    NextPage,
    PreviousPage,
    ReorderPageNext,
    ReorderPagePrevious,
    CloseGroup,
    MinimizeGroup,
    MaximizeGroup,
    RestoreGroup,
    Count,
};

struct HybridShortcutTriggers final
{
    std::function<void()> dock;
    std::function<void()> dockPage;
    std::function<void()> moveGroup;
    std::function<void()> resizeActiveSplit;
    std::function<void()> resizeGroup;
    std::function<void()> nextPage;
    std::function<void()> previousPage;
    std::function<void()> reorderPageNext;
    std::function<void()> reorderPagePrevious;
    std::function<void()> closeGroup;
    std::function<void()> minimizeGroup;
    std::function<void()> maximizeGroup;
    std::function<void()> restoreGroup;
};

class HybridShortcutManager final
{
public:
    explicit HybridShortcutManager(HybridShortcutTriggers triggers,
                                   bool registerGlobally = true);
    ~HybridShortcutManager();

    HybridShortcutManager(const HybridShortcutManager &) = delete;
    HybridShortcutManager &operator=(const HybridShortcutManager &) = delete;

    [[nodiscard]] static QKeySequence defaultShortcut(HybridShortcutAction action);
    [[nodiscard]] static QString stableActionId(HybridShortcutAction action);
    [[nodiscard]] QAction *action(HybridShortcutAction action) const noexcept;
    [[nodiscard]] QAction *keyboardDockAction() const noexcept;
    [[nodiscard]] QAction *keyboardDockPageAction() const noexcept;
    [[nodiscard]] QAction *keyboardMoveGroupAction() const noexcept;
    [[nodiscard]] QAction *keyboardResizeActiveSplitAction() const noexcept;
    [[nodiscard]] QAction *keyboardResizeGroupAction() const noexcept;
    [[nodiscard]] QAction *keyboardNextPageAction() const noexcept;
    [[nodiscard]] QAction *keyboardPreviousPageAction() const noexcept;
    [[nodiscard]] QAction *keyboardReorderPageNextAction() const noexcept;
    [[nodiscard]] QAction *keyboardReorderPagePreviousAction() const noexcept;
    [[nodiscard]] QAction *keyboardCloseGroupAction() const noexcept;
    [[nodiscard]] QAction *keyboardMinimizeGroupAction() const noexcept;
    [[nodiscard]] QAction *keyboardMaximizeGroupAction() const noexcept;
    [[nodiscard]] QAction *keyboardRestoreGroupAction() const noexcept;
    [[nodiscard]] bool registered() const noexcept { return m_registered; }

private:
    static constexpr std::size_t ActionCount =
        static_cast<std::size_t>(HybridShortcutAction::Count);

    HybridShortcutTriggers m_triggers;
    std::array<std::unique_ptr<QAction>, ActionCount> m_actions;
    bool m_registered = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
