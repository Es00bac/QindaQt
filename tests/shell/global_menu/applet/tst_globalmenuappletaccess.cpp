// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/global_menu/applet/globalmenuappletaccess.h>

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Shell::GlobalMenu;
using namespace QindaQt::Shell::GlobalMenu::Protocol;

namespace {

MenuTree fixtureTree()
{
    MenuItem enabledAction;
    enabledAction.id = QStringLiteral("fileNewAction");
    enabledAction.kind = MenuItemKind::Action;
    enabledAction.text = QStringLiteral("New");
    enabledAction.enabled = true;
    enabledAction.visible = true;

    MenuItem disabledAction;
    disabledAction.id = QStringLiteral("fileQuitAction");
    disabledAction.kind = MenuItemKind::Action;
    disabledAction.text = QStringLiteral("Quit");
    disabledAction.enabled = false;
    disabledAction.visible = true;

    MenuItem separator;
    separator.id = QStringLiteral("fileSep1");
    separator.kind = MenuItemKind::Separator;

    MenuItem submenu;
    submenu.id = QStringLiteral("fileMenu");
    submenu.kind = MenuItemKind::Submenu;
    submenu.text = QStringLiteral("File");
    submenu.children = {enabledAction, separator, disabledAction};

    MenuTree tree;
    tree.ownerWindowId = QUuid::createUuid();
    tree.epoch = QUuid::createUuid();
    tree.revision = 1;
    tree.items = {submenu};
    return tree;
}

} // namespace

class GlobalMenuAppletAccessTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void startsUnavailableWithNoItems();
    void publishTreeMakesAvailableAndProjectsTopLevel();
    void publishUnavailableClearsItems();
    void publishInvalidTreeFailsClosedToUnavailable();
    void activateAfterInvalidPublishEmitsNothing();
    void activateOnUnavailableFacadeEmitsNothing();
    void activateOnUnknownIdEmitsNothing();
    void activateOnDisabledActionEmitsNothing();
    void activateOnNestedEnabledActionEmits();
    void activateOnSubmenuEmitsNothing();
};

void GlobalMenuAppletAccessTests::startsUnavailableWithNoItems()
{
    GlobalMenuAppletAccess access;
    QVERIFY(!access.available());
    QVERIFY(access.items().isEmpty());
}

void GlobalMenuAppletAccessTests::publishTreeMakesAvailableAndProjectsTopLevel()
{
    GlobalMenuAppletAccess access;
    QSignalSpy availableSpy(&access, &GlobalMenuAppletAccess::availableChanged);
    QSignalSpy itemsSpy(&access, &GlobalMenuAppletAccess::itemsChanged);

    access.publishTree(fixtureTree());
    QVERIFY(access.available());
    QCOMPARE(availableSpy.count(), 1);
    QCOMPARE(itemsSpy.count(), 1);

    const QVariantList items = access.items();
    QCOMPARE(items.size(), 1);
    const QVariantMap topLevel = items.first().toMap();
    QCOMPARE(topLevel.value(QStringLiteral("id")).toString(), QStringLiteral("fileMenu"));
    QCOMPARE(topLevel.value(QStringLiteral("text")).toString(), QStringLiteral("File"));
}

void GlobalMenuAppletAccessTests::publishUnavailableClearsItems()
{
    GlobalMenuAppletAccess access;
    access.publishTree(fixtureTree());
    QVERIFY(access.available());

    access.publishUnavailable();
    QVERIFY(!access.available());
    QVERIFY(access.items().isEmpty());
}

void GlobalMenuAppletAccessTests::publishInvalidTreeFailsClosedToUnavailable()
{
    GlobalMenuAppletAccess access;
    access.publishTree(fixtureTree());
    QVERIFY(access.available());

    // A previously good publication must not excuse a later malformed one:
    // duplicate ids make this tree canonically invalid, and the facade must
    // fall back to "no menu" rather than admit any of its content.
    MenuTree invalid = fixtureTree();
    const MenuItem duplicate = invalid.items.constFirst();
    invalid.items.append(duplicate);
    access.publishTree(invalid);
    QVERIFY(!access.available());
    QVERIFY(access.items().isEmpty());
}

void GlobalMenuAppletAccessTests::activateAfterInvalidPublishEmitsNothing()
{
    GlobalMenuAppletAccess access;
    access.publishTree(fixtureTree());

    MenuTree invalid = fixtureTree();
    const MenuItem duplicate = invalid.items.constFirst();
    invalid.items.append(duplicate);
    access.publishTree(invalid);

    QSignalSpy spy(&access, &GlobalMenuAppletAccess::activationRequested);
    access.activate(QStringLiteral("fileNewAction"));
    QCOMPARE(spy.count(), 0);
}

void GlobalMenuAppletAccessTests::activateOnUnavailableFacadeEmitsNothing()
{
    GlobalMenuAppletAccess access;
    QSignalSpy spy(&access, &GlobalMenuAppletAccess::activationRequested);
    access.activate(QStringLiteral("fileNewAction"));
    QCOMPARE(spy.count(), 0);
}

void GlobalMenuAppletAccessTests::activateOnUnknownIdEmitsNothing()
{
    GlobalMenuAppletAccess access;
    access.publishTree(fixtureTree());
    QSignalSpy spy(&access, &GlobalMenuAppletAccess::activationRequested);
    access.activate(QStringLiteral("does-not-exist"));
    QCOMPARE(spy.count(), 0);
}

void GlobalMenuAppletAccessTests::activateOnDisabledActionEmitsNothing()
{
    GlobalMenuAppletAccess access;
    access.publishTree(fixtureTree());
    QSignalSpy spy(&access, &GlobalMenuAppletAccess::activationRequested);
    access.activate(QStringLiteral("fileQuitAction"));
    QCOMPARE(spy.count(), 0);
}

void GlobalMenuAppletAccessTests::activateOnNestedEnabledActionEmits()
{
    GlobalMenuAppletAccess access;
    access.publishTree(fixtureTree());
    QSignalSpy spy(&access, &GlobalMenuAppletAccess::activationRequested);
    access.activate(QStringLiteral("fileNewAction"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.constFirst().constFirst().toString(), QStringLiteral("fileNewAction"));
}

void GlobalMenuAppletAccessTests::activateOnSubmenuEmitsNothing()
{
    GlobalMenuAppletAccess access;
    access.publishTree(fixtureTree());
    QSignalSpy spy(&access, &GlobalMenuAppletAccess::activationRequested);
    access.activate(QStringLiteral("fileMenu"));
    QCOMPARE(spy.count(), 0);
}

QTEST_APPLESS_MAIN(GlobalMenuAppletAccessTests)
#include "tst_globalmenuappletaccess.moc"
