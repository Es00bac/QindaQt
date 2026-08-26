// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridshortcutmanager.h"

#include <QAction>
#include <QTest>

using QindaQt::Compositor::KWinIntegration::HybridShortcutManager;
using QindaQt::Compositor::KWinIntegration::HybridShortcutAction;
using QindaQt::Compositor::KWinIntegration::HybridShortcutTriggers;

namespace {

QVector<HybridShortcutAction> allActions()
{
    return {
        HybridShortcutAction::Dock,
        HybridShortcutAction::DockPage,
        HybridShortcutAction::MoveGroup,
        HybridShortcutAction::ResizeActiveSplit,
        HybridShortcutAction::ResizeGroup,
        HybridShortcutAction::NextPage,
        HybridShortcutAction::PreviousPage,
        HybridShortcutAction::ReorderPageNext,
        HybridShortcutAction::ReorderPagePrevious,
        HybridShortcutAction::CloseGroup,
        HybridShortcutAction::MinimizeGroup,
        HybridShortcutAction::MaximizeGroup,
        HybridShortcutAction::RestoreGroup,
    };
}

} // namespace

class HybridShortcutManagerTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesStableActionsDefaultsAndDispatches();
    void emptyTriggersRemainSafeAndUnregistered();
};

void HybridShortcutManagerTest::exposesStableActionsDefaultsAndDispatches()
{
    QVector<HybridShortcutAction> dispatched;
    HybridShortcutManager manager(
        {.dock = [&] { dispatched.append(HybridShortcutAction::Dock); },
         .dockPage = [&] { dispatched.append(HybridShortcutAction::DockPage); },
         .moveGroup = [&] { dispatched.append(HybridShortcutAction::MoveGroup); },
         .resizeActiveSplit = [&] {
             dispatched.append(HybridShortcutAction::ResizeActiveSplit);
         },
         .resizeGroup = [&] { dispatched.append(HybridShortcutAction::ResizeGroup); },
         .nextPage = [&] { dispatched.append(HybridShortcutAction::NextPage); },
         .previousPage = [&] {
             dispatched.append(HybridShortcutAction::PreviousPage);
         },
         .reorderPageNext = [&] {
             dispatched.append(HybridShortcutAction::ReorderPageNext);
         },
         .reorderPagePrevious = [&] {
             dispatched.append(HybridShortcutAction::ReorderPagePrevious);
         },
         .closeGroup = [&] { dispatched.append(HybridShortcutAction::CloseGroup); },
         .minimizeGroup = [&] {
             dispatched.append(HybridShortcutAction::MinimizeGroup);
         },
         .maximizeGroup = [&] {
             dispatched.append(HybridShortcutAction::MaximizeGroup);
         },
         .restoreGroup = [&] {
             dispatched.append(HybridShortcutAction::RestoreGroup);
         }},
        false);

    const QStringList objectNames{
        QStringLiteral("qindaqt_keyboard_dock"),
        QStringLiteral("qindaqt_keyboard_dock_page"),
        QStringLiteral("qindaqt_keyboard_move_group"),
        QStringLiteral("qindaqt_keyboard_resize_active_split"),
        QStringLiteral("qindaqt_keyboard_resize_group"),
        QStringLiteral("qindaqt_keyboard_next_page"),
        QStringLiteral("qindaqt_keyboard_previous_page"),
        QStringLiteral("qindaqt_keyboard_reorder_page_next"),
        QStringLiteral("qindaqt_keyboard_reorder_page_previous"),
        QStringLiteral("qindaqt_keyboard_close_group"),
        QStringLiteral("qindaqt_keyboard_minimize_group"),
        QStringLiteral("qindaqt_keyboard_maximize_group"),
        QStringLiteral("qindaqt_keyboard_restore_group"),
    };
    const auto kinds = allActions();
    const QVector<QKeySequence> defaults{
        QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_D),
        QKeySequence(Qt::META | Qt::CTRL | Qt::SHIFT | Qt::Key_D),
        QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_M),
        QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_S),
        QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_R),
        QKeySequence(Qt::META | Qt::CTRL | Qt::Key_PageDown),
        QKeySequence(Qt::META | Qt::CTRL | Qt::Key_PageUp),
        QKeySequence(Qt::META | Qt::CTRL | Qt::SHIFT | Qt::Key_PageDown),
        QKeySequence(Qt::META | Qt::CTRL | Qt::SHIFT | Qt::Key_PageUp),
        QKeySequence(Qt::META | Qt::CTRL | Qt::SHIFT | Qt::Key_Q),
        QKeySequence(Qt::META | Qt::CTRL | Qt::SHIFT | Qt::Key_N),
        QKeySequence(Qt::META | Qt::CTRL | Qt::SHIFT | Qt::Key_X),
        QKeySequence(Qt::META | Qt::CTRL | Qt::SHIFT | Qt::Key_U),
    };

    QCOMPARE(kinds.size(), objectNames.size());
    QCOMPARE(kinds.size(), defaults.size());
    for (qsizetype index = 0; index < kinds.size(); ++index) {
        auto *const action = manager.action(kinds[index]);
        QVERIFY(action);
        QCOMPARE(action->objectName(), objectNames[index]);
        QCOMPARE(HybridShortcutManager::stableActionId(kinds[index]),
                 objectNames[index]);
        QVERIFY(!action->text().isEmpty());
        QCOMPARE(HybridShortcutManager::defaultShortcut(kinds[index]), defaults[index]);
        action->trigger();
    }
    QCOMPARE(manager.keyboardDockAction(), manager.action(HybridShortcutAction::Dock));
    QCOMPARE(manager.keyboardDockPageAction(),
             manager.action(HybridShortcutAction::DockPage));
    QCOMPARE(manager.keyboardNextPageAction(),
             manager.action(HybridShortcutAction::NextPage));
    QCOMPARE(manager.keyboardPreviousPageAction(),
             manager.action(HybridShortcutAction::PreviousPage));
    QCOMPARE(manager.keyboardReorderPageNextAction(),
             manager.action(HybridShortcutAction::ReorderPageNext));
    QCOMPARE(manager.keyboardReorderPagePreviousAction(),
             manager.action(HybridShortcutAction::ReorderPagePrevious));
    QCOMPARE(manager.keyboardCloseGroupAction(),
             manager.action(HybridShortcutAction::CloseGroup));
    QCOMPARE(manager.keyboardMinimizeGroupAction(),
             manager.action(HybridShortcutAction::MinimizeGroup));
    QCOMPARE(manager.keyboardMaximizeGroupAction(),
             manager.action(HybridShortcutAction::MaximizeGroup));
    QCOMPARE(manager.keyboardRestoreGroupAction(),
             manager.action(HybridShortcutAction::RestoreGroup));
    QVERIFY(!manager.action(HybridShortcutAction::Count));
    QVERIFY(!manager.registered());
    QCOMPARE(dispatched, kinds);
}

void HybridShortcutManagerTest::emptyTriggersRemainSafeAndUnregistered()
{
    HybridShortcutManager manager(HybridShortcutTriggers{}, false);
    for (const auto kind : allActions()) {
        manager.action(kind)->trigger();
    }
    QVERIFY(!manager.registered());
}

QTEST_MAIN(HybridShortcutManagerTest)
#include "tst_hybridshortcutmanager.moc"
