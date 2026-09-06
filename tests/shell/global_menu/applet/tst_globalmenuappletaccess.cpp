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

    MenuItem hiddenAction;
    hiddenAction.id = QStringLiteral("fileHiddenAction");
    hiddenAction.kind = MenuItemKind::Action;
    hiddenAction.text = QStringLiteral("Secret");
    hiddenAction.enabled = true;
    hiddenAction.visible = false;

    MenuItem separator;
    separator.id = QStringLiteral("fileSep1");
    separator.kind = MenuItemKind::Separator;

    MenuItem submenu;
    submenu.id = QStringLiteral("fileMenu");
    submenu.kind = MenuItemKind::Submenu;
    submenu.text = QStringLiteral("File");
    submenu.children = {enabledAction, separator, disabledAction, hiddenAction};

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
    void staleRenderedGenerationCannotActivateReusedId();
    void startsUnavailableWithNoItems();
    void publishTreeMakesAvailableAndProjectsTopLevel();
    void projectionCarriesHonestKindsAndOmitsHiddenEntries();
    void projectionCarriesNestedPopupEntries();
    void degradedPhaseClearsStaleTruth();
    void publishUnavailableClearsItems();
    void publishInvalidTreeFailsClosedToUnavailable();
    void activateAfterInvalidPublishEmitsNothing();
    void activateOnUnavailableFacadeEmitsNothing();
    void activateOnUnknownIdEmitsNothing();
    void activateOnDisabledActionEmitsNothing();
    void activateOnNestedEnabledActionEmits();
    void activateOnSubmenuEmitsNothing();
    void activateOnHiddenActionEmitsNothing();
    void rendererLeasesAreReferenceCounted();
    void transitionRetainsProjectionButFencesActivation();
    void endTransitionRestoresRetainedProjection();
    void transitionOnEmptyProjectionIsUnavailable();
    void publishTreeDuringTransitionReplacesProjection();
};

void GlobalMenuAppletAccessTests::rendererLeasesAreReferenceCounted()
{
    GlobalMenuAppletAccess access;
    QSignalSpy changed(&access, &GlobalMenuAppletAccess::rendererPresentChanged);
    QVERIFY(!access.rendererPresent());
    access.attachRenderer();
    access.attachRenderer();
    QVERIFY(access.rendererPresent());
    QCOMPARE(changed.size(), 1);
    access.detachRenderer();
    QVERIFY(access.rendererPresent());
    QCOMPARE(changed.size(), 1);
    access.detachRenderer();
    QVERIFY(!access.rendererPresent());
    QCOMPARE(changed.size(), 2);
    access.detachRenderer();
    QCOMPARE(changed.size(), 2);
}

void GlobalMenuAppletAccessTests::staleRenderedGenerationCannotActivateReusedId()
{
    GlobalMenuAppletAccess access;
    MenuTree tree = fixtureTree();
    access.publishTree(tree);
    const QString first = access.items().first().toMap().value("generation").toString();
    QSignalSpy activations(&access, &GlobalMenuAppletAccess::activationRequested);
    QSignalSpy publications(&access, &GlobalMenuAppletAccess::itemsChanged);
    tree.items[0].children[0].text = QStringLiteral("Different action");
    ++tree.revision;
    access.publishTree(tree);
    access.activate(QStringLiteral("fileNewAction"), first);
    QCOMPARE(activations.count(), 0);
    const QString current = access.items().first().toMap().value("generation").toString();
    access.activate(QStringLiteral("fileNewAction"), current);
    QCOMPARE(activations.count(), 1);
    // Identical labels with new provider lineage still invalidate old gestures.
    tree.epoch = QUuid::createUuid();
    access.publishTree(tree);
    QCOMPARE(publications.count(), 2);
    access.activate(QStringLiteral("fileNewAction"), current);
    QCOMPARE(activations.count(), 1);
}

void GlobalMenuAppletAccessTests::startsUnavailableWithNoItems()
{
    GlobalMenuAppletAccess access;
    QVERIFY(!access.available());
    QVERIFY(access.items().isEmpty());
}

void GlobalMenuAppletAccessTests::projectionCarriesNestedPopupEntries()
{
    GlobalMenuAppletAccess access;
    access.publishTree(fixtureTree());

    const QVariantMap submenu = access.items().constFirst().toMap();
    const QVariantList children = submenu.value(QStringLiteral("children")).toList();
    QCOMPARE(children.size(), 3);
    QCOMPARE(children.at(0).toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("fileNewAction"));
    QCOMPARE(children.at(1).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("separator"));
    QCOMPARE(children.at(2).toMap().value(QStringLiteral("enabled")).toBool(),
             false);
}

void GlobalMenuAppletAccessTests::degradedPhaseClearsStaleTruth()
{
    GlobalMenuAppletAccess access;
    access.publishTree(fixtureTree());
    access.publishDegraded(QStringLiteral("registrar-name-owned"));

    QVERIFY(!access.available());
    QVERIFY(access.items().isEmpty());
    QCOMPARE(access.phase(), QStringLiteral("degraded"));
    QCOMPARE(access.reasonCode(), QStringLiteral("registrar-name-owned"));
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
    QCOMPARE(topLevel.value(QStringLiteral("kind")).toString(), QStringLiteral("submenu"));
}

void GlobalMenuAppletAccessTests::projectionCarriesHonestKindsAndOmitsHiddenEntries()
{
    // A realistic top level: File submenu, an enabled direct action, and a
    // hidden action. The projection must present all three roles truthfully:
    // submenu marked as such, action activatable, hidden omitted entirely.
    const MenuTree base = fixtureTree();
    const MenuItem nestedEnabledAction = base.items.constFirst().children.at(0);
    MenuItem direct = nestedEnabledAction;
    direct.id = QStringLiteral("topLevelDirectAction");
    const MenuItem nestedDisabledAction = base.items.constFirst().children.at(2);
    MenuItem hidden = nestedDisabledAction;
    hidden.enabled = true;
    hidden.visible = false;
    hidden.id = QStringLiteral("topLevelHiddenAction");

    MenuTree tree;
    tree.items = {base.items.constFirst(), direct, hidden};

    GlobalMenuAppletAccess access;
    access.publishTree(tree);
    QVERIFY(access.available());

    const QVariantList items = access.items();
    QCOMPARE(items.size(), 2);
    const QVariantMap submenuEntry = items.at(0).toMap();
    QCOMPARE(submenuEntry.value(QStringLiteral("id")).toString(), QStringLiteral("fileMenu"));
    QCOMPARE(submenuEntry.value(QStringLiteral("kind")).toString(), QStringLiteral("submenu"));
    QVERIFY(submenuEntry.value(QStringLiteral("enabled")).toBool());

    const QVariantMap actionEntry = items.at(1).toMap();
    QCOMPARE(actionEntry.value(QStringLiteral("id")).toString(),
             QStringLiteral("topLevelDirectAction"));
    QCOMPARE(actionEntry.value(QStringLiteral("kind")).toString(), QStringLiteral("action"));
    QVERIFY(actionEntry.value(QStringLiteral("enabled")).toBool());

    // The hidden top-level entry is absent, not a disabled ghost.
    for (const QVariant &entry : items) {
        QVERIFY(entry.toMap().value(QStringLiteral("id")).toString()
                != QStringLiteral("topLevelHiddenAction"));
    }

    // A hidden nested action must not be invocable through its id either.
    QSignalSpy spy(&access, &GlobalMenuAppletAccess::activationRequested);
    access.activate(QStringLiteral("fileHiddenAction"));
    QCOMPARE(spy.count(), 0);
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

void GlobalMenuAppletAccessTests::activateOnHiddenActionEmitsNothing()
{
    GlobalMenuAppletAccess access;
    access.publishTree(fixtureTree());
    QSignalSpy spy(&access, &GlobalMenuAppletAccess::activationRequested);
    access.activate(QStringLiteral("fileHiddenAction"));
    QCOMPARE(spy.count(), 0);
}

void GlobalMenuAppletAccessTests::transitionRetainsProjectionButFencesActivation()
{
    GlobalMenuAppletAccess access;
    access.publishTree(fixtureTree());
    const QVariantList retained = access.items();
    const QString generation =
        retained.first().toMap().value(QStringLiteral("generation")).toString();
    QSignalSpy activations(&access, &GlobalMenuAppletAccess::activationRequested);
    QSignalSpy publications(&access, &GlobalMenuAppletAccess::itemsChanged);

    access.beginTransition();
    QVERIFY(!access.available());
    QCOMPARE(access.phase(), QStringLiteral("loading"));
    // The projection is retained verbatim so the panel slot and delegates
    // survive the provider swap...
    QCOMPARE(access.items(), retained);
    QCOMPARE(publications.count(), 0);
    // ...but nothing in it is actionable, even with the current generation.
    access.activate(QStringLiteral("fileNewAction"), generation);
    access.activate(QStringLiteral("fileNewAction"));
    QCOMPARE(activations.count(), 0);
}

void GlobalMenuAppletAccessTests::endTransitionRestoresRetainedProjection()
{
    GlobalMenuAppletAccess access;
    access.publishTree(fixtureTree());
    const QVariantList retained = access.items();
    const QString generation =
        retained.first().toMap().value(QStringLiteral("generation")).toString();
    QSignalSpy publications(&access, &GlobalMenuAppletAccess::itemsChanged);
    QSignalSpy activations(&access, &GlobalMenuAppletAccess::activationRequested);

    access.beginTransition();
    access.endTransition();
    QVERIFY(access.available());
    QCOMPARE(access.phase(), QStringLiteral("ready"));
    QCOMPARE(access.items(), retained);
    QCOMPARE(publications.count(), 0);
    access.activate(QStringLiteral("fileNewAction"), generation);
    QCOMPARE(activations.count(), 1);

    // A mismatched publisher still ends the retained placeholder truthfully.
    access.beginTransition();
    access.publishUnavailable();
    QVERIFY(!access.available());
    QVERIFY(access.items().isEmpty());
    access.endTransition();
    QVERIFY(!access.available());
}

void GlobalMenuAppletAccessTests::transitionOnEmptyProjectionIsUnavailable()
{
    GlobalMenuAppletAccess access;
    access.beginTransition();
    QVERIFY(!access.available());
    QVERIFY(access.items().isEmpty());
    QCOMPARE(access.phase(), QStringLiteral("unavailable"));
    access.endTransition();
    QVERIFY(!access.available());
}

void GlobalMenuAppletAccessTests::publishTreeDuringTransitionReplacesProjection()
{
    GlobalMenuAppletAccess access;
    access.publishTree(fixtureTree());
    access.beginTransition();
    QVERIFY(!access.available());

    MenuTree replacement = fixtureTree();
    replacement.epoch = QUuid::createUuid();
    replacement.items.first().text = QStringLiteral("Edit");
    access.publishTree(replacement);
    QVERIFY(access.available());
    QCOMPARE(access.phase(), QStringLiteral("ready"));
    QCOMPARE(access.items().first().toMap().value(QStringLiteral("text")).toString(),
             QStringLiteral("Edit"));
}

QTEST_APPLESS_MAIN(GlobalMenuAppletAccessTests)
#include "tst_globalmenuappletaccess.moc"
