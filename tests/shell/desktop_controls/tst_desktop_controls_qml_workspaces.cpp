// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop_controls_qml_test_support.h"
#include "desktop_controls_test_support.h"

#include "power_applet_controller.h"
#include "qindaqt/shell/desktop_controls/system_status_controller.h"
#include "support/fake_power_transport.h"

#include <QAccessible>
#include <QQmlExtensionPlugin>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_DesktopControlsPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt;
using namespace QindaQt::Shell::DesktopControls;
using namespace QindaQt::Tests::DesktopControls;

class DesktopControlsQmlWorkspaceTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void workspaceSwitcherIsKeyboardOperableAndAccessible();
    void workspaceTilesShowNamesAndUnavailableStateStaysIconOnly();
    void showDesktopButtonTogglesThroughTheFacade();
    void systemStatusOpensAWindowPopupWithLaneControls();
};

void DesktopControlsQmlWorkspaceTests::workspaceSwitcherIsKeyboardOperableAndAccessible()
{
    WorkspaceStack workspaces;
    workspaces.publishReady();
    AppletHost host;
    QString error;
    QVERIFY2(host.create(QStringLiteral("WorkspaceSwitcherApplet"), &workspaces.controller, &error),
             qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());

    const auto tiles = host.visualItemsNamed(QStringLiteral("workspaceTile"));
    QCOMPARE(tiles.size(), 3);
    QVERIFY(host.item->implicitWidth() > 60);
    QAccessibleInterface *second = QAccessible::queryAccessibleInterface(tiles.at(1));
    QVERIFY(second != nullptr);
    QCOMPARE(second->role(), QAccessible::RadioButton);
    QVERIFY(second->text(QAccessible::Name).contains(QStringLiteral("Code")));
    QVERIFY(second->state().checked);
    QAccessibleInterface *first = QAccessible::queryAccessibleInterface(tiles.at(0));
    QVERIFY(!first->state().checked);

    tiles.at(0)->forceActiveFocus();
    QVERIFY(tiles.at(0)->hasActiveFocus());
    QTest::keyClick(host.window.get(), Qt::Key_Right);
    QVERIFY(tiles.at(1)->hasActiveFocus());
    QTest::keyClick(host.window.get(), Qt::Key_Right);
    QVERIFY(tiles.at(2)->hasActiveFocus());
    QTest::keyClick(host.window.get(), Qt::Key_Space);
    QCOMPARE(workspaces.transport.switchRequests.size(), 1);
    QCOMPARE(workspaces.transport.switchRequests.constLast().desktopId, QStringLiteral("ws-3"));

    // While the switch is pending every non-current tile is disabled.
    QVERIFY(!tiles.at(0)->isEnabled());
    workspaces.transport.finishSwitch(workspaces.transport.switchRequests.constLast(), true);
    workspaces.transport.change(QStringLiteral(":1.7"));
    workspaces.transport.reply(workspaces.transport.snapshotRequests.constLast(),
                               QindaQt::Tests::Workspaces::fixtureSnapshot(QStringLiteral("ws-3")));
    QTRY_VERIFY(tiles.at(0)->isEnabled());
    QVERIFY(QAccessible::queryAccessibleInterface(host.visualItemsNamed(
                QStringLiteral("workspaceTile")).at(2))->state().checked);
}

void DesktopControlsQmlWorkspaceTests::workspaceTilesShowNamesAndUnavailableStateStaysIconOnly()
{
    WorkspaceStack workspaces;
    workspaces.publishReady();
    AppletHost host;
    QString error;
    QVERIFY2(host.create(QStringLiteral("WorkspaceTilesApplet"), &workspaces.controller, &error, true),
             qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    const auto labels = host.visualItemsNamed(QStringLiteral("workspaceTileLabel"));
    QCOMPARE(labels.size(), 3);
    QCOMPARE(labels.at(0)->property("text").toString(), QStringLiteral("Main"));
    QCOMPARE(labels.at(2)->property("text").toString(), QStringLiteral("Workspace 3"));

    workspaces.transport.announce(QString{}, QStringLiteral("compositor-owner-changed"));
    QTRY_COMPARE(host.visualItemsNamed(QStringLiteral("workspaceTile")).size(), 0);
    auto *placeholder = host.child<QQuickItem>(QStringLiteral("workspaceStripPlaceholder"));
    QVERIFY(placeholder != nullptr);
    QVERIFY(placeholder->isVisible());
    auto *strip = host.child<QQuickItem>(QStringLiteral("workspaceTilesStrip"));
    QVERIFY(strip != nullptr);
    QAccessibleInterface *stripInterface = QAccessible::queryAccessibleInterface(strip);
    QVERIFY(stripInterface != nullptr);
    QVERIFY(stripInterface->text(QAccessible::Description)
                .contains(QStringLiteral("compositor-owner-changed")));

    AppletHost detached;
    QVERIFY2(detached.create(QStringLiteral("WorkspaceTilesApplet"), nullptr, &error), qPrintable(error));
    QCOMPARE(detached.visualItemsNamed(QStringLiteral("workspaceTile")).size(), 0);
}

void DesktopControlsQmlWorkspaceTests::showDesktopButtonTogglesThroughTheFacade()
{
    WorkspaceStack workspaces;
    workspaces.publishReady();
    AppletHost host;
    QString error;
    QVERIFY2(host.create(QStringLiteral("ShowDesktopApplet"), &workspaces.controller, &error),
             qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    auto *button = host.child<QQuickItem>(QStringLiteral("showDesktopButton"));
    QVERIFY(button != nullptr);
    QAccessibleInterface *interface = QAccessible::queryAccessibleInterface(button);
    QVERIFY(interface != nullptr);
    QCOMPARE(interface->role(), QAccessible::Button);
    QCOMPARE(interface->text(QAccessible::Name), QStringLiteral("Show desktop"));
    QVERIFY(!interface->text(QAccessible::Description).isEmpty());

    button->forceActiveFocus();
    QVERIFY(button->hasActiveFocus());
    QTest::keyClick(host.window.get(), Qt::Key_Return);
    QCOMPARE(workspaces.transport.showDesktopRequests.size(), 1);
    QCOMPARE(workspaces.transport.showDesktopRequests.constLast().showing, true);
    QVERIFY(!button->isEnabled()); // pending
    workspaces.transport.finishShowDesktop(workspaces.transport.showDesktopRequests.constLast(), true);
    workspaces.transport.change(QStringLiteral(":1.7"));
    workspaces.transport.reply(workspaces.transport.snapshotRequests.constLast(),
                               QindaQt::Tests::Workspaces::fixtureSnapshot(QStringLiteral("ws-2"), true));
    QTRY_VERIFY(button->isEnabled());
    QCOMPARE(interface->text(QAccessible::Name), QStringLiteral("Hide desktop"));
    QVERIFY(interface->state().checked);

    AppletHost detached;
    QVERIFY2(detached.create(QStringLiteral("ShowDesktopApplet"), nullptr, &error), qPrintable(error));
    auto *disabled = detached.child<QQuickItem>(QStringLiteral("showDesktopButton"));
    QVERIFY(disabled != nullptr);
    QVERIFY(!disabled->isEnabled());
}

void DesktopControlsQmlWorkspaceTests::systemStatusOpensAWindowPopupWithLaneControls()
{
    QindaQt::Tests::FakePowerTransport transport;
    Power::PowerClient client(&transport);
    Shell::PowerApplet::PowerAppletController power(&client, true, true);
    client.start();
    transport.announceOwner(QStringLiteral(":1.42"));
    transport.reply(transport.fetches.constLast(), QindaQt::Tests::powerClientSnapshot());
    SystemStatusController status(nullptr, nullptr, &power, {false, false, false, false, true, true});

    AppletHost host;
    QString error;
    QVERIFY2(host.create(QStringLiteral("SystemStatusApplet"), &status, &error), qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    QCOMPARE(host.visualItemsNamed(QStringLiteral("systemStatusLaneIcon")).size(), 1);
    auto *summary = host.child<QQuickItem>(QStringLiteral("systemStatusSummary"));
    QVERIFY(summary != nullptr);
    QAccessibleInterface *interface = QAccessible::queryAccessibleInterface(summary);
    QVERIFY(interface != nullptr);
    QCOMPARE(interface->role(), QAccessible::Button);
    QVERIFY(interface->text(QAccessible::Name).startsWith(QStringLiteral("System status: ")));

    summary->forceActiveFocus();
    QTest::keyClick(host.window.get(), Qt::Key_Space);
    QObject *popup = host.child<QObject>(QStringLiteral("systemStatusPopup"));
    QVERIFY(popup != nullptr);
    QTRY_VERIFY(popup->property("opened").toBool());
    QCOMPARE(popup->property("popupType").toInt(), PopupTypeWindow);
    QTRY_COMPARE(host.visualItemsNamed(QStringLiteral("statusLaneProfileButton")).size(), 2);
    QQuickItem *powerSaver = nullptr;
    for (QQuickItem *button : host.visualItemsNamed(QStringLiteral("statusLaneProfileButton"))) {
        if (button->property("text").toString().startsWith(QStringLiteral("Power Saver"))) {
            powerSaver = button;
        }
    }
    QVERIFY(powerSaver != nullptr);
    QVERIFY(powerSaver->isEnabled());
    powerSaver->forceActiveFocus();
    QTRY_VERIFY(powerSaver->hasActiveFocus());
    keyClickFocused(host, Qt::Key_Space);
    QTRY_COMPARE(transport.operations.size(), 1);
    QCOMPARE(transport.operations.constLast().request.profileId, QStringLiteral("power-saver"));

    keyClickFocused(host, Qt::Key_Escape);
    QTRY_VERIFY(!popup->property("opened").toBool());
}

QTEST_MAIN(DesktopControlsQmlWorkspaceTests)
#include "tst_desktop_controls_qml_workspaces.moc"
