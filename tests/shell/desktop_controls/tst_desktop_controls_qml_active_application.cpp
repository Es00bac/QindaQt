// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop_controls_qml_test_support.h"
#include "desktop_controls_test_support.h"

#include "qindaqt/shell/desktop_controls/active_application_controller.h"

#include <qindaqt/shell/global_menu/applet/globalmenuappletaccess.h>

#include <QQmlExtensionPlugin>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_DesktopControlsPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt;
using namespace QindaQt::Shell::DesktopControls;
using namespace QindaQt::Tests::DesktopControls;

namespace {

// The editor row loses focus to the terminal row, which is restored.
QVector<ShellTaskList::TaskWindowFact> terminalActiveFacts()
{
    auto facts = TaskListStack::activeEditorFacts();
    facts[0].active = false;
    facts[1].active = true;
    facts[1].minimized = false;
    return facts;
}

// Popup.Window may finish its exit asynchronously, so a retired popup is one
// that is neither visible nor still reporting opened.
bool popupShown(const QObject *popup)
{
    return popup->property("visible").toBool() || popup->property("opened").toBool();
}

} // namespace

class DesktopControlsQmlActiveApplicationTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void popupRetiresWithItsDisplayedTask();
    void namesTheDesktopMenuApplicationWithoutAWindow();
};

void DesktopControlsQmlActiveApplicationTests::popupRetiresWithItsDisplayedTask()
{
    TaskListStack tasks;
    ShellTaskListApplet::TaskListAppletController taskList(tasks.source, tasks.authority,
                                                          tasks.port, {true, true, true});
    ActiveApplicationController active(&taskList, {true, true});
    QVERIFY(tasks.publish(TaskListStack::activeEditorFacts()) > 0);

    AppletHost host;
    QString error;
    QVERIFY2(host.create(QStringLiteral("ActiveApplicationApplet"), &active, &error),
             qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    auto *summary = host.child<QQuickItem>(QStringLiteral("activeApplicationSummary"));
    QObject *popup = host.child<QObject>(QStringLiteral("activeApplicationPopup"));
    auto *title = host.child<QQuickItem>(QStringLiteral("activeApplicationTitle"));
    auto *minimize = host.child<QQuickItem>(QStringLiteral("activeApplicationMinimize"));
    auto *closeRow = host.child<QQuickItem>(QStringLiteral("activeApplicationClose"));
    QVERIFY(summary != nullptr && popup != nullptr && title != nullptr);
    QVERIFY(minimize != nullptr && closeRow != nullptr);
    const auto openPopup = [summary, popup] {
        QVERIFY(QMetaObject::invokeMethod(summary, "openActions"));
        QTRY_VERIFY(popup->property("opened").toBool());
    };

    openPopup();
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(title->property("text").toString(), QStringLiteral("Title w-editor"));

    // Regression control: the facade reprojects with the same task identity
    // and displayed revision. That emission must not dismiss the popup.
    const quint64 editorRevision = active.revision();
    QSignalSpy stateSpy(&active, &ActiveApplicationController::stateChanged);
    Q_EMIT tasks.authority.stateChanged();
    QVERIFY(stateSpy.size() >= 1);
    QCOMPARE(active.taskId(), QStringLiteral("w-editor"));
    QCOMPARE(active.revision(), editorRevision);
    QTest::qWait(50);
    QVERIFY2(popup->property("opened").toBool(),
             "an unchanged task identity and revision must not dismiss the popup");

    // Focus moves to the terminal while the editor's popup is open. Checked on
    // the publishing turn, before any event could reach a row.
    tasks.publish(terminalActiveFacts());
    QCOMPARE(active.taskId(), QStringLiteral("w-terminal"));
    QVERIFY2(!popupShown(popup),
             "the editor popup must close before a row can act on the terminal");
    // A row activation left over from the transition still dispatches nothing.
    QVERIFY(QMetaObject::invokeMethod(minimize, "activated"));
    QVERIFY(QMetaObject::invokeMethod(closeRow, "activated"));
    QTRY_VERIFY(!popup->property("opened").toBool());
    QCOMPARE(tasks.port.calls.size(), 0);

    // Reopening binds to the newly published task.
    openPopup();
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(title->property("text").toString(), QStringLiteral("Title w-terminal"));

    // A new displayed revision of the same task retires the popup as well.
    const quint64 republished = tasks.publish(terminalActiveFacts());
    QCOMPARE(active.taskId(), QStringLiteral("w-terminal"));
    QCOMPARE(active.revision(), republished);
    QVERIFY2(!popupShown(popup), "a new displayed revision must retire the popup");
    QTRY_VERIFY(!popup->property("opened").toBool());

    // The active window disappears: the popup closes and cannot reopen.
    openPopup();
    if (QTest::currentTestFailed())
        return;
    auto noneActive = terminalActiveFacts();
    noneActive[1].active = false;
    tasks.publish(noneActive);
    QVERIFY(!active.hasActiveWindow());
    QVERIFY2(!popupShown(popup), "losing the active window must retire the popup");
    QTRY_VERIFY(!popup->property("opened").toBool());
    QVERIFY(QMetaObject::invokeMethod(summary, "openActions"));
    QTest::qWait(50);
    QVERIFY(!popup->property("opened").toBool());
    QCOMPARE(tasks.port.calls.size(), 0);

    // The terminal returns: reopen and act on exactly its displayed revision.
    const quint64 terminalRevision = tasks.publish(terminalActiveFacts());
    openPopup();
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(title->property("text").toString(), QStringLiteral("Title w-terminal"));
    QTRY_VERIFY(minimize->hasActiveFocus());
    keyClickFocused(host, Qt::Key_Return);
    QTRY_COMPARE(tasks.port.calls.size(), 1);
    QCOMPARE(tasks.port.lastCall().method, QStringLiteral("executeTaskIntent"));
    QCOMPARE(tasks.port.lastCall().firstId, QStringLiteral("w-terminal"));
    QCOMPARE(tasks.port.lastCall().revision, terminalRevision);
    QTRY_VERIFY(!popup->property("opened").toBool());
}

void DesktopControlsQmlActiveApplicationTests::namesTheDesktopMenuApplicationWithoutAWindow()
{
    // ADR-0260: the top-left name reads "File Manager" while the global menu
    // shows the desktop menu, and "Desktop" again once it does not.
    TaskListStack tasks;
    ShellTaskListApplet::TaskListAppletController taskList(tasks.source, tasks.authority,
                                                          tasks.port, {true, true, true});
    Shell::GlobalMenu::GlobalMenuAppletAccess globalMenu;
    ActiveApplicationController active(&taskList, {true, true}, &globalMenu);

    AppletHost host;
    QString error;
    QVERIFY2(host.create(QStringLiteral("ActiveApplicationApplet"), &active, &error),
             qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    auto *name = host.child<QQuickItem>(QStringLiteral("activeApplicationName"));
    auto *summary = host.child<QQuickItem>(QStringLiteral("activeApplicationSummary"));
    auto *icon = host.child<QQuickItem>(QStringLiteral("activeApplicationIcon"));
    QVERIFY(name != nullptr && summary != nullptr && icon != nullptr);
    QCOMPARE(name->property("text").toString(), QStringLiteral("Desktop"));

    Shell::GlobalMenu::Protocol::MenuTree tree;
    Shell::GlobalMenu::Protocol::MenuItem lock;
    lock.id = QStringLiteral("desktop.lock-screen");
    lock.text = QStringLiteral("Lock Screen");
    Shell::GlobalMenu::Protocol::MenuItem application;
    application.id = QStringLiteral("desktop.menu.application");
    application.kind = Shell::GlobalMenu::Protocol::MenuItemKind::Submenu;
    application.text = QStringLiteral("File Manager");
    application.children = {lock};
    tree.revision = 1;
    tree.items = {application};
    globalMenu.publishDesktopTree(tree, QStringLiteral("File Manager"),
                                  QStringLiteral("org.qindaqt.FileManager"));
    QTRY_COMPARE(name->property("text").toString(), QStringLiteral("File Manager"));
    QCOMPARE(icon->property("name").toString(), QStringLiteral("org.qindaqt.FileManager"));
    // No window: the indicator names the menu but offers no window actions.
    QVERIFY(!summary->isEnabled());

    globalMenu.withdrawDesktopTree();
    QTRY_COMPARE(name->property("text").toString(), QStringLiteral("Desktop"));
}

QTEST_MAIN(DesktopControlsQmlActiveApplicationTests)
#include "tst_desktop_controls_qml_active_application.moc"
