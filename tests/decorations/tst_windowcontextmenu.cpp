// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindawindowcontextmenu.h"

#include <QAction>
#include <QTest>

using namespace QindaQt::Decoration;

namespace {

QAction *actionNamed(QMenu &menu, const QString &name)
{
    for (auto *action : menu.actions()) {
        if (action->objectName() == name) {
            return action;
        }
    }
    return nullptr;
}

} // namespace

class WindowContextMenuTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void reflectsCapabilitiesAndDefersCommands();
    void presentsRestoreAndRollDownFromCurrentState();
};

void WindowContextMenuTest::reflectsCapabilitiesAndDefersCommands()
{
    QVector<WindowContextCommand> commands;
    QindaWindowContextMenu menu(
        [&commands](WindowContextCommand command) { commands.append(command); });
    menu.prepare({
        .canMinimize = true,
        .canMaximize = true,
        .maximized = false,
        .canShade = false,
        .shaded = false,
        .onAllWorkspaces = true,
        .keepAbove = false,
        .keepBelow = true,
        .canClose = true,
    });

    auto *const minimize =
        actionNamed(menu, QStringLiteral("qindaqt-window-minimize"));
    auto *const maximize =
        actionNamed(menu, QStringLiteral("qindaqt-window-toggle-maximized"));
    auto *const shade =
        actionNamed(menu, QStringLiteral("qindaqt-window-toggle-shaded"));
    auto *const allWorkspaces =
        actionNamed(menu, QStringLiteral("qindaqt-window-all-workspaces"));
    auto *const keepAbove =
        actionNamed(menu, QStringLiteral("qindaqt-window-keep-above"));
    auto *const keepBelow =
        actionNamed(menu, QStringLiteral("qindaqt-window-keep-below"));
    auto *const close = actionNamed(menu, QStringLiteral("qindaqt-window-close"));
    QVERIFY(minimize && maximize && shade && allWorkspaces && keepAbove &&
            keepBelow && close);
    QCOMPARE(minimize->text(), QStringLiteral("Minimize"));
    QCOMPARE(maximize->text(), QStringLiteral("Maximize"));
    QVERIFY(!shade->isVisible());
    QVERIFY(allWorkspaces->isChecked());
    QVERIFY(!keepAbove->isChecked());
    QVERIFY(keepBelow->isChecked());

    menu.show();
    minimize->trigger();
    QVERIFY(commands.isEmpty());
    menu.hide();
    QTRY_COMPARE(commands,
                 QVector<WindowContextCommand>{WindowContextCommand::Minimize});
}

void WindowContextMenuTest::presentsRestoreAndRollDownFromCurrentState()
{
    QindaWindowContextMenu menu([](WindowContextCommand) {});
    menu.prepare({
        .canMinimize = false,
        .canMaximize = true,
        .maximized = true,
        .canShade = true,
        .shaded = true,
        .onAllWorkspaces = false,
        .keepAbove = true,
        .keepBelow = false,
        .canClose = false,
    });

    auto *const minimize =
        actionNamed(menu, QStringLiteral("qindaqt-window-minimize"));
    auto *const maximize =
        actionNamed(menu, QStringLiteral("qindaqt-window-toggle-maximized"));
    auto *const shade =
        actionNamed(menu, QStringLiteral("qindaqt-window-toggle-shaded"));
    auto *const keepAbove =
        actionNamed(menu, QStringLiteral("qindaqt-window-keep-above"));
    auto *const close = actionNamed(menu, QStringLiteral("qindaqt-window-close"));
    QVERIFY(minimize && maximize && shade && keepAbove && close);
    QVERIFY(!minimize->isEnabled());
    QCOMPARE(maximize->text(), QStringLiteral("Restore"));
    QVERIFY(shade->isVisible());
    QCOMPARE(shade->text(), QStringLiteral("Roll Down"));
    QVERIFY(keepAbove->isChecked());
    QVERIFY(!close->isEnabled());
}

QTEST_MAIN(WindowContextMenuTest)
#include "tst_windowcontextmenu.moc"
