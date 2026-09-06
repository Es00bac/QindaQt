// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwingroupcontextmenu.h"

#include <QAction>
#include <QTest>

using namespace QindaQt::Compositor::KWinIntegration;

namespace {

QAction *actionNamed(QMenu &menu, const QString &name)
{
    for (auto *action : menu.actions()) {
        if (action->objectName() == name) {
            return action;
        }
        if (action->menu()) {
            if (auto *nested = actionNamed(*action->menu(), name)) {
                return nested;
            }
        }
    }
    return nullptr;
}

GroupContextMenuState populatedState()
{
    return {
        .activeMemberId = QStringLiteral("member-active"),
        .keepAbove = true,
        .keepBelow = false,
        .pinnedToAllWorkspaces = false,
        .onAllActivities = false,
        .workspaces = {
            {QStringLiteral("one"), QStringLiteral("Workspace One"), true},
            {QStringLiteral("two"), QStringLiteral("Workspace Two"), false},
        },
        .activities = {
            {QStringLiteral("work"), QStringLiteral("Work"), true},
            {QStringLiteral("play"), QStringLiteral("Play"), false},
        },
        .outputs = {
            {QStringLiteral("left"), QStringLiteral("Left Display"), true},
            {QStringLiteral("right"), QStringLiteral("Right Display"), false},
        },
    };
}

} // namespace

class KWinGroupContextMenuTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void reflectsStateAndDispatchesTypedCommands();
    void defersDispatchWithStableContainerIdentity();
    void rejectsStaleOrMalformedState();
};

void KWinGroupContextMenuTest::reflectsStateAndDispatchesTypedCommands()
{
    QVector<std::pair<QString, GroupContextMenuCommand>> commands;
    KWinGroupContextMenu menu(
        [](const QString &containerId, QString *)
            -> std::optional<GroupContextMenuState> {
            return containerId == QStringLiteral("group-a")
                ? std::optional(populatedState()) : std::nullopt;
        },
        [&commands](const QString &containerId,
                    const GroupContextMenuCommand &command,
                    QString *) {
            commands.append({containerId, command});
            return true;
        });

    QString error;
    QVERIFY2(menu.prepare(QStringLiteral("group-a"), &error),
             qPrintable(error));
    auto *const above = actionNamed(
        menu, QStringLiteral("qindaqt-context-keep-above"));
    auto *const arrange = actionNamed(
        menu, QStringLiteral("qindaqt-context-arrange"));
    auto *const detach = actionNamed(
        menu, QStringLiteral("qindaqt-context-detach-active"));
    auto *const ungroup = actionNamed(
        menu, QStringLiteral("qindaqt-context-ungroup"));
    auto *const workspace = actionNamed(
        menu, QStringLiteral("qindaqt-context-workspace-two"));
    auto *const activity = actionNamed(
        menu, QStringLiteral("qindaqt-context-activity-play"));
    auto *const output = actionNamed(
        menu, QStringLiteral("qindaqt-context-output-right"));
    QVERIFY(arrange && detach && ungroup && above && workspace && activity && output);
    QCOMPARE(arrange->text(), QStringLiteral("Arrange windows"));
    QCOMPARE(detach->text(), QStringLiteral("Detach active window"));
    QCOMPARE(ungroup->text(), QStringLiteral("Ungroup"));
    QVERIFY(above->isChecked());
    QVERIFY(!workspace->isChecked());

    menu.show();
    QVERIFY(menu.isVisible());
    arrange->trigger();
    detach->trigger();
    ungroup->trigger();
    above->trigger();
    workspace->trigger();
    activity->trigger();
    output->trigger();
    QVERIFY(commands.isEmpty());
    menu.hide();
    QVERIFY(!menu.isVisible());
    QTRY_COMPARE(commands.size(), 7);
    const GroupContextMenuCommand expectedArrange{
        GroupContextMenuCommandKind::ArrangeWindows,
        QStringLiteral("member-active"), true};
    const GroupContextMenuCommand expectedDetach{
        GroupContextMenuCommandKind::DetachActiveWindow,
        QStringLiteral("member-active"), true};
    const GroupContextMenuCommand expectedUngroup{
        GroupContextMenuCommandKind::Ungroup, {}, true};
    const std::pair<QString, GroupContextMenuCommand> expectedAbove{
        QStringLiteral("group-a"),
        {GroupContextMenuCommandKind::SetKeepAbove, {}, false}};
    const GroupContextMenuCommand expectedWorkspace{
        GroupContextMenuCommandKind::ToggleWorkspace,
        QStringLiteral("two"), true};
    const GroupContextMenuCommand expectedActivity{
        GroupContextMenuCommandKind::ToggleActivity,
        QStringLiteral("play"), true};
    const GroupContextMenuCommand expectedOutput{
        GroupContextMenuCommandKind::MoveToOutput,
        QStringLiteral("right"), true};
    QCOMPARE(commands[0].second, expectedArrange);
    QCOMPARE(commands[1].second, expectedDetach);
    QCOMPARE(commands[2].second, expectedUngroup);
    QCOMPARE(commands[3], expectedAbove);
    QCOMPARE(commands[4].second, expectedWorkspace);
    QCOMPARE(commands[5].second, expectedActivity);
    QCOMPARE(commands[6].second, expectedOutput);

    QCoreApplication::processEvents();
    QCOMPARE(commands.size(), 7);
}

void KWinGroupContextMenuTest::defersDispatchWithStableContainerIdentity()
{
    QVector<std::pair<QString, GroupContextMenuCommand>> commands;
    KWinGroupContextMenu *menuAddress = nullptr;
    bool handlerSawHiddenMenu = false;
    KWinGroupContextMenu menu(
        [](const QString &containerId, QString *)
            -> std::optional<GroupContextMenuState> {
            if (containerId == QStringLiteral("group-a")
                || containerId == QStringLiteral("group-b")) {
                return populatedState();
            }
            return std::nullopt;
        },
        [&commands, &menuAddress, &handlerSawHiddenMenu](
            const QString &containerId,
            const GroupContextMenuCommand &command,
            QString *) {
            handlerSawHiddenMenu = menuAddress && !menuAddress->isVisible();
            commands.append({containerId, command});
            return true;
        });
    menuAddress = &menu;

    QString error;
    QVERIFY2(menu.prepare(QStringLiteral("group-a"), &error),
             qPrintable(error));
    auto *const above = actionNamed(
        menu, QStringLiteral("qindaqt-context-keep-above"));
    QVERIFY(above);
    menu.show();
    QVERIFY(menu.isVisible());
    above->trigger();
    QVERIFY(commands.isEmpty());
    menu.hide();
    QVERIFY(!menu.isVisible());

    // Reopening the reusable popup before queued delivery must neither run the
    // old request under live popup chrome nor retarget it to the new group.
    QVERIFY2(menu.prepare(QStringLiteral("group-b"), &error),
             qPrintable(error));
    menu.show();
    QVERIFY(menu.isVisible());
    QCoreApplication::processEvents();
    QVERIFY(commands.isEmpty());
    menu.hide();
    QTRY_COMPARE(commands.size(), 1);
    QVERIFY(handlerSawHiddenMenu);
    QCOMPARE(commands.constFirst().first, QStringLiteral("group-a"));
    const GroupContextMenuCommand expected{
        GroupContextMenuCommandKind::SetKeepAbove, {}, false};
    QCOMPARE(commands.constFirst().second, expected);

    QCoreApplication::processEvents();
    QCOMPARE(commands.size(), 1);
}

void KWinGroupContextMenuTest::rejectsStaleOrMalformedState()
{
    KWinGroupContextMenu missing({}, {});
    QString error;
    QVERIFY(!missing.prepare(QStringLiteral("group-a"), &error));
    QVERIFY(!error.isEmpty());

    auto malformed = populatedState();
    malformed.keepBelow = true;
    KWinGroupContextMenu contradictory(
        [malformed](const QString &, QString *) {
            return std::optional(malformed);
        },
        [](const QString &, const GroupContextMenuCommand &, QString *) {
            return true;
        });
    QVERIFY(!contradictory.prepare(QStringLiteral("group-a"), &error));
    QVERIFY(error.contains(QStringLiteral("contradictory")));

    malformed.keepBelow = false;
    malformed.outputs.append(malformed.outputs.constFirst());
    KWinGroupContextMenu duplicate(
        [malformed](const QString &, QString *) {
            return std::optional(malformed);
        },
        [](const QString &, const GroupContextMenuCommand &, QString *) {
            return true;
        });
    QVERIFY(!duplicate.prepare(QStringLiteral("group-a"), &error));
    QVERIFY(error.contains(QStringLiteral("output")));
}

QTEST_MAIN(KWinGroupContextMenuTest)
#include "tst_kwingroupcontextmenu.moc"
