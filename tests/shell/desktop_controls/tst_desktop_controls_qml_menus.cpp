// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop_controls_qml_test_support.h"
#include "desktop_controls_test_support.h"

#include "qindaqt/shell/desktop_controls/command_search_controller.h"
#include "qindaqt/shell/desktop_controls/places_controller.h"
#include "qindaqt/shell/desktop_controls/quick_launch_controller.h"
#include "qindaqt/shell/desktop_controls/system_menu_controller.h"

#include <QAccessible>
#include <QQmlExtensionPlugin>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_DesktopControlsPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt;
using namespace QindaQt::Shell::DesktopControls;
using namespace QindaQt::Tests::DesktopControls;

class DesktopControlsQmlMenuTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void systemMenuOpensAWindowPopupAndDispatchesThroughFacades();
    void commandPaletteSearchesByKeyboardAndActivates();
    void placesMenuOpensFoldersThroughTheSeam();
    void quickLaunchDockUsesOnlyPersistedPins();
};

void DesktopControlsQmlMenuTests::systemMenuOpensAWindowPopupAndDispatchesThroughFacades()
{
    LauncherStack stack;
    QVERIFY(stack.addEntry(QStringLiteral("org.qindaqt.Settings.desktop"),
                           QStringLiteral("System Settings"), QStringLiteral("/bin/true")));
    QVERIFY(stack.scanner.start());
    Shell::Launcher::LauncherAppletController launcher(&stack.scanner, nullptr, &stack.executor, true);
    StubSessionActions session;
    SystemMenuController menu(&session, &launcher, true,
                              SystemMenuController::defaultSettingsEntryId(), QStringLiteral("0.1.0"));

    AppletHost host;
    QString error;
    QVERIFY2(host.create(QStringLiteral("SystemMenuApplet"), &menu, &error), qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    auto *button = host.child<QQuickItem>(QStringLiteral("systemMenuButton"));
    QVERIFY(button != nullptr);
    QAccessibleInterface *interface = QAccessible::queryAccessibleInterface(button);
    QVERIFY(interface != nullptr);
    QCOMPARE(interface->role(), QAccessible::Button);
    QCOMPARE(interface->text(QAccessible::Name), QStringLiteral("System menu"));

    host.focus(button);
    QTest::keyClick(host.window.get(), Qt::Key_Space);
    QObject *popup = host.child<QObject>(QStringLiteral("systemMenuPopup"));
    QVERIFY(popup != nullptr);
    QTRY_VERIFY(popup->property("opened").toBool());
    QCOMPARE(popup->property("popupType").toInt(), PopupTypeWindow);

    auto *about = host.child<QQuickItem>(QStringLiteral("systemMenuAbout"));
    auto *settings = host.child<QQuickItem>(QStringLiteral("systemMenuSettings"));
    auto *lock = host.child<QQuickItem>(QStringLiteral("systemMenuLock"));
    auto *suspend = host.child<QQuickItem>(QStringLiteral("systemMenuSuspend"));
    QVERIFY(about != nullptr && settings != nullptr && lock != nullptr && suspend != nullptr);
    QTRY_VERIFY(about->hasActiveFocus());
    QCOMPARE(QAccessible::queryAccessibleInterface(settings)->role(), QAccessible::MenuItem);
    QVERIFY(QAccessible::queryAccessibleInterface(about)->text(QAccessible::Description)
                .contains(QStringLiteral("0.1.0")));
    QVERIFY(settings->isEnabled());
    QVERIFY(lock->isEnabled());
    QVERIFY(!suspend->isEnabled()); // stub reports canSuspend = false

    keyClickFocused(host, Qt::Key_Down);
    QTRY_VERIFY(settings->hasActiveFocus());
    keyClickFocused(host, Qt::Key_Down);
    QTRY_VERIFY(lock->hasActiveFocus());
    keyClickFocused(host, Qt::Key_Return);
    QCOMPARE(session.requests, QStringList{QStringLiteral("lock")});
    QTRY_VERIFY(!popup->property("opened").toBool());

    host.focus(button);
    QTest::keyClick(host.window.get(), Qt::Key_Return);
    QTRY_VERIFY(popup->property("opened").toBool());
    QTRY_VERIFY(about->hasActiveFocus());
    keyClickFocused(host, Qt::Key_Down);
    QTRY_VERIFY(settings->hasActiveFocus());
    keyClickFocused(host, Qt::Key_Space);
    QCOMPARE(stack.spawner.requests.size(), 1);
    QCOMPARE(stack.spawner.requests.constFirst().program, QStringLiteral("/bin/true"));
    QTRY_VERIFY(!popup->property("opened").toBool());

    // Destructive actions confirm first and never dispatch on open.
    host.focus(button);
    QTest::keyClick(host.window.get(), Qt::Key_Space);
    QTRY_VERIFY(popup->property("opened").toBool());
    auto *powerOff = host.child<QQuickItem>(QStringLiteral("systemMenuPowerOff"));
    QVERIFY(powerOff != nullptr);
    powerOff->forceActiveFocus();
    QTRY_VERIFY(powerOff->hasActiveFocus());
    keyClickFocused(host, Qt::Key_Return);
    QObject *confirmation = host.child<QObject>(QStringLiteral("systemMenuConfirmation"));
    QVERIFY(confirmation != nullptr);
    QTRY_VERIFY(confirmation->property("opened").toBool());
    QCOMPARE(confirmation->property("popupType").toInt(), PopupTypeWindow);
    QCOMPARE(session.requests.size(), 1);
    keyClickFocused(host, Qt::Key_Escape);
    QTRY_VERIFY(!confirmation->property("opened").toBool());
    QCOMPARE(session.requests.size(), 1);
}

void DesktopControlsQmlMenuTests::commandPaletteSearchesByKeyboardAndActivates()
{
    LauncherStack stack;
    QVERIFY(stack.addEntry(QStringLiteral("editor.desktop"), QStringLiteral("Text Editor"),
                           QStringLiteral("/bin/true")));
    QVERIFY(stack.addEntry(QStringLiteral("terminal.desktop"), QStringLiteral("Terminal"),
                           QStringLiteral("/bin/false")));
    QVERIFY(stack.scanner.start());
    Shell::Launcher::LauncherAppletController launcher(&stack.scanner, nullptr, &stack.executor, true);
    WorkspaceStack workspaces;
    workspaces.publishReady();
    CommandSearchController palette({&launcher, nullptr, nullptr, &workspaces.controller},
                                    {true, false, true, true, true},
                                    {CommandSourceKind::Applications, CommandSourceKind::Workspaces});

    AppletHost host;
    QString error;
    QVERIFY2(host.create(QStringLiteral("CommandPaletteApplet"), &palette, &error), qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    auto *button = host.child<QQuickItem>(QStringLiteral("commandPaletteButton"));
    QVERIFY(button != nullptr);
    QCOMPARE(QAccessible::queryAccessibleInterface(button)->text(QAccessible::Name),
             QStringLiteral("Command palette"));
    host.focus(button);
    QTest::keyClick(host.window.get(), Qt::Key_Space);
    QObject *popup = host.child<QObject>(QStringLiteral("commandPalettePopup"));
    QVERIFY(popup != nullptr);
    QTRY_VERIFY(popup->property("opened").toBool());
    QCOMPARE(popup->property("popupType").toInt(), PopupTypeWindow);
    auto *field = host.child<QQuickItem>(QStringLiteral("commandSearchField"));
    QVERIFY(field != nullptr);
    QTRY_VERIFY(field->hasActiveFocus());
    QCOMPARE(QAccessible::queryAccessibleInterface(field)->role(), QAccessible::EditableText);

    // Browsing shows the workspace commands; typing narrows to applications.
    QTRY_COMPARE(host.visualItemsNamed(QStringLiteral("commandSearchRow")).size(), 4);
    keyClicksFocused(host, QStringLiteral("term"));
    QCOMPARE(palette.query(), QStringLiteral("term"));
    QTRY_COMPARE(host.visualItemsNamed(QStringLiteral("commandSearchRow")).size(), 1);
    QQuickItem *row = host.visualItemsNamed(QStringLiteral("commandSearchRow")).constFirst();
    QCOMPARE(QAccessible::queryAccessibleInterface(row)->role(), QAccessible::MenuItem);
    QVERIFY(QAccessible::queryAccessibleInterface(row)->text(QAccessible::Name)
                .startsWith(QStringLiteral("Terminal")));
    keyClickFocused(host, Qt::Key_Down);
    QTRY_VERIFY(row->hasActiveFocus());
    keyClickFocused(host, Qt::Key_Up);
    QTRY_VERIFY(field->hasActiveFocus());
    keyClickFocused(host, Qt::Key_Return);
    QCOMPARE(stack.spawner.requests.size(), 1);
    QCOMPARE(stack.spawner.requests.constFirst().program, QStringLiteral("/bin/false"));
    QTRY_VERIFY(!popup->property("opened").toBool());
    QCOMPARE(palette.query(), QString{}); // closing resets the query

    // Reopen: workspace switch from the browse list dispatches with the
    // displayed revision.
    host.focus(button);
    QTest::keyClick(host.window.get(), Qt::Key_Space);
    QTRY_VERIFY(popup->property("opened").toBool());
    QTRY_VERIFY(field->hasActiveFocus());
    keyClickFocused(host, Qt::Key_Down);
    const auto rows = host.visualItemsNamed(QStringLiteral("commandSearchRow"));
    QVERIFY(rows.size() >= 2);
    QTRY_VERIFY(rows.at(0)->hasActiveFocus());
    QVERIFY(!rows.at(1)->isEnabled()); // current workspace is not switchable
    keyClickFocused(host, Qt::Key_Space);
    QCOMPARE(workspaces.transport.switchRequests.size(), 1);
    QCOMPARE(workspaces.transport.switchRequests.constLast().desktopId, QStringLiteral("ws-1"));
    QTRY_VERIFY(!popup->property("opened").toBool());
}

void DesktopControlsQmlMenuTests::placesMenuOpensFoldersThroughTheSeam()
{
    RecordingFolderOpener opener;
    PlacesController places(&opener, true,
                            {{QStringLiteral("home"), QStringLiteral("Home"),
                              QStringLiteral("/home/fixture"), QStringLiteral("user-home")},
                             {QStringLiteral("documents"), QStringLiteral("Documents"),
                              QStringLiteral("/home/fixture/Documents"), QStringLiteral("folder-documents")}});
    AppletHost host;
    QString error;
    QVERIFY2(host.create(QStringLiteral("PlacesMenuApplet"), &places, &error), qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    auto *button = host.child<QQuickItem>(QStringLiteral("placesMenuButton"));
    QVERIFY(button != nullptr);
    host.focus(button);
    QTest::keyClick(host.window.get(), Qt::Key_Space);
    QObject *popup = host.child<QObject>(QStringLiteral("placesMenuPopup"));
    QVERIFY(popup != nullptr);
    QTRY_VERIFY(popup->property("opened").toBool());
    QCOMPARE(popup->property("popupType").toInt(), PopupTypeWindow);
    const auto rows = host.visualItemsNamed(QStringLiteral("placesMenuRow"));
    QCOMPARE(rows.size(), 2);
    QTRY_VERIFY(rows.at(0)->hasActiveFocus());
    QCOMPARE(QAccessible::queryAccessibleInterface(rows.at(1))->text(QAccessible::Name),
             QStringLiteral("Documents, /home/fixture/Documents"));
    keyClickFocused(host, Qt::Key_Down);
    QTRY_VERIFY(rows.at(1)->hasActiveFocus());
    keyClickFocused(host, Qt::Key_Return);
    QCOMPARE(opener.opened, QStringList{QStringLiteral("/home/fixture/Documents")});
    QTRY_VERIFY(!popup->property("opened").toBool());

    opener.nextResult = {false, QStringLiteral("no file manager")};
    host.focus(button);
    QTest::keyClick(host.window.get(), Qt::Key_Space);
    QTRY_VERIFY(popup->property("opened").toBool());
    QTRY_VERIFY(rows.at(0)->hasActiveFocus());
    keyClickFocused(host, Qt::Key_Return);
    QCOMPARE(opener.opened.size(), 2);
    QVERIFY(popup->property("opened").toBool()); // failure keeps the menu open
    auto *feedback = host.child<QQuickItem>(QStringLiteral("controlPopupFeedback"));
    QVERIFY(feedback != nullptr);
    QTRY_VERIFY(feedback->isVisible());
    QVERIFY(feedback->property("text").toString().contains(QStringLiteral("no file manager")));
    keyClickFocused(host, Qt::Key_Escape);
    QTRY_VERIFY(!popup->property("opened").toBool());
}

void DesktopControlsQmlMenuTests::quickLaunchDockUsesOnlyPersistedPins()
{
    LauncherStack stack;
    QVERIFY(stack.addEntry(QStringLiteral("editor.desktop"), QStringLiteral("Fixture Editor"),
                           QStringLiteral("/bin/true")));
    QVERIFY(stack.scanner.start());
    stack.publishPinned({QStringLiteral("editor")});
    Shell::Launcher::LauncherAppletController launcher(&stack.scanner, &stack.persistence,
                                                        &stack.executor, true);
    QuickLaunchController quickLaunch(&launcher, true);

    AppletHost host;
    QString error;
    QVERIFY2(host.create(QStringLiteral("QuickLaunchApplet"), &quickLaunch, &error, false,
                          true, 2), qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    QCOMPARE(host.item->implicitHeight(), 56.0);
    const auto entries = host.visualItemsNamed(QStringLiteral("quickLaunchEntry"));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.constFirst()->width(), 56.0);
    auto *icon = entries.constFirst()->findChild<QQuickItem *>(
        QStringLiteral("quickLaunchEntryIcon"));
    QVERIFY(icon != nullptr);
    QCOMPARE(icon->property("size").toInt(), 40);
    QCOMPARE(icon->width(), 40.0);
    QCOMPARE(icon->height(), 40.0);
    auto *tooltip = entries.constFirst()->findChild<QObject *>(
        QStringLiteral("quickLaunchEntryTooltip"));
    QVERIFY(tooltip != nullptr);
    QCOMPARE(tooltip->property("text").toString(), QStringLiteral("Fixture Editor"));

    entries.constFirst()->forceActiveFocus();
    keyClickFocused(host, Qt::Key_Return);
    QCOMPARE(stack.spawner.requests.size(), 1);
}

QTEST_MAIN(DesktopControlsQmlMenuTests)
#include "tst_desktop_controls_qml_menus.moc"
