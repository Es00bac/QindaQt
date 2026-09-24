// SPDX-License-Identifier: GPL-3.0-or-later

// ADR-0260: the facade's desktop channel and provider confirmations. The
// application channel's own behaviour stays pinned by
// tst_globalmenuappletaccess.cpp; these rows prove the desktop channel only
// ever fills the empty state, routes to its own signal, and cannot leak into
// the application's invocation path or hosting acknowledgment facts.

#include <qindaqt/shell/global_menu/applet/globalmenuappletaccess.h>

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Shell::GlobalMenu;
using namespace QindaQt::Shell::GlobalMenu::Protocol;

namespace {

MenuItem action(const char *id, const char *text, bool enabled = true)
{
    MenuItem item;
    item.id = QString::fromLatin1(id);
    item.kind = MenuItemKind::Action;
    item.text = QString::fromLatin1(text);
    item.enabled = enabled;
    return item;
}

MenuItem submenu(const char *id, const char *text, QList<MenuItem> children)
{
    MenuItem item;
    item.id = QString::fromLatin1(id);
    item.kind = MenuItemKind::Submenu;
    item.text = QString::fromLatin1(text);
    item.children = std::move(children);
    return item;
}

MenuTree applicationTree()
{
    MenuTree tree;
    tree.ownerWindowId = QUuid::createUuid();
    tree.epoch = QUuid::createUuid();
    tree.revision = 1;
    tree.items = {submenu("fileMenu", "File", {action("fileNew", "New")})};
    return tree;
}

MenuTree desktopTree(const char *label = "Lock Screen")
{
    MenuTree tree;
    tree.revision = 1;
    tree.items = {submenu("desktop.app", "File Manager",
                          {action("desktop.lock-screen", label),
                           action("desktop.restart", "Restart…", false)})};
    return tree;
}

QString firstGeneration(const GlobalMenuAppletAccess &access)
{
    return access.items().value(0).toMap().value(QStringLiteral("generation")).toString();
}

} // namespace

class GlobalMenuAppletAccessDesktopTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void desktopTreeFillsTheEmptyState();
    void applicationTreeReplacesDesktopImmediately();
    void desktopReturnsWhenApplicationChannelEmpties();
    void applicationTransitionKeepsPriorityOverDesktop();
    void desktopActivationUsesItsOwnSignalAndGeneration();
    void inertDesktopTreeIsPaintedButNotActionable();
    void identicalDesktopRepublishKeepsGeneration();
    void invalidDesktopTreeWithdrawsFailClosed();
    void applicationFactsIgnoreThePresentedDesktop();
    void confirmationResolvesOnlyTheOutstandingTokenOnce();
    void newConfirmationDeclinesTheUnansweredOne();
};

void GlobalMenuAppletAccessDesktopTests::desktopTreeFillsTheEmptyState()
{
    GlobalMenuAppletAccess access;
    QSignalSpy shown(&access, &GlobalMenuAppletAccess::desktopMenuShownChanged);
    access.publishDesktopTree(desktopTree(), QStringLiteral("File Manager"),
                              QStringLiteral("org.qindaqt.FileManager"));
    QVERIFY(access.available());
    QCOMPARE(access.phase(), QStringLiteral("ready"));
    QVERIFY(access.desktopMenuShown());
    QCOMPARE(access.desktopMenuTitle(), QStringLiteral("File Manager"));
    QCOMPARE(access.desktopMenuIconName(), QStringLiteral("org.qindaqt.FileManager"));
    QCOMPARE(access.items().size(), 1);
    QCOMPARE(access.items().first().toMap().value(QStringLiteral("text")).toString(),
             QStringLiteral("File Manager"));
    QCOMPARE(shown.size(), 1);
}

void GlobalMenuAppletAccessDesktopTests::applicationTreeReplacesDesktopImmediately()
{
    GlobalMenuAppletAccess access;
    access.publishDesktopTree(desktopTree(), QStringLiteral("File Manager"), {});
    QSignalSpy shown(&access, &GlobalMenuAppletAccess::desktopMenuShownChanged);
    access.publishTree(applicationTree());
    QVERIFY(access.available());
    QVERIFY(!access.desktopMenuShown());
    QVERIFY(access.desktopMenuTitle().isEmpty());
    QCOMPARE(access.items().first().toMap().value(QStringLiteral("text")).toString(),
             QStringLiteral("File"));
    QCOMPARE(shown.size(), 1);
}

void GlobalMenuAppletAccessDesktopTests::desktopReturnsWhenApplicationChannelEmpties()
{
    GlobalMenuAppletAccess access;
    access.publishTree(applicationTree());
    access.publishDesktopTree(desktopTree(), QStringLiteral("File Manager"), {});
    // The application's menu owns the presentation while it exists.
    QVERIFY(!access.desktopMenuShown());
    QCOMPARE(access.items().first().toMap().value(QStringLiteral("text")).toString(),
             QStringLiteral("File"));
    access.publishUnavailable();
    QVERIFY(access.desktopMenuShown());
    QVERIFY(access.available());
    QCOMPARE(access.items().first().toMap().value(QStringLiteral("text")).toString(),
             QStringLiteral("File Manager"));
    access.publishDegraded(QStringLiteral("registrar-name-owned"));
    QVERIFY(access.desktopMenuShown());
    QCOMPARE(access.reasonCode(), QString{});
    access.withdrawDesktopTree();
    QVERIFY(!access.desktopMenuShown());
    QVERIFY(!access.available());
    QCOMPARE(access.phase(), QStringLiteral("degraded"));
    QCOMPARE(access.reasonCode(), QStringLiteral("registrar-name-owned"));
    QVERIFY(access.items().isEmpty());
}

void GlobalMenuAppletAccessDesktopTests::applicationTransitionKeepsPriorityOverDesktop()
{
    GlobalMenuAppletAccess access;
    access.publishTree(applicationTree());
    access.publishDesktopTree(desktopTree(), QStringLiteral("File Manager"), {});
    access.beginTransition();
    // A retained application projection stays painted and inert; the desktop
    // menu never flashes in between two application menus.
    QVERIFY(!access.desktopMenuShown());
    QVERIFY(!access.available());
    QCOMPARE(access.phase(), QStringLiteral("loading"));
    QCOMPARE(access.items().first().toMap().value(QStringLiteral("text")).toString(),
             QStringLiteral("File"));
}

void GlobalMenuAppletAccessDesktopTests::desktopActivationUsesItsOwnSignalAndGeneration()
{
    GlobalMenuAppletAccess access;
    access.publishTree(applicationTree());
    const QString applicationGeneration = firstGeneration(access);
    access.publishUnavailable();
    access.publishDesktopTree(desktopTree(), QStringLiteral("File Manager"), {});
    const QString desktopGeneration = firstGeneration(access);
    QVERIFY(desktopGeneration != applicationGeneration);

    QSignalSpy application(&access, &GlobalMenuAppletAccess::activationRequested);
    QSignalSpy desktop(&access, &GlobalMenuAppletAccess::desktopActivationRequested);
    // A delegate rendered from the earlier application projection is stale.
    access.activate(QStringLiteral("desktop.lock-screen"), applicationGeneration);
    QCOMPARE(desktop.size(), 0);
    // Disabled and unknown desktop ids never leave the facade.
    access.activate(QStringLiteral("desktop.restart"), desktopGeneration);
    access.activate(QStringLiteral("fileNew"), desktopGeneration);
    access.activate(QStringLiteral("desktop.app"), desktopGeneration);
    QCOMPARE(desktop.size(), 0);
    access.activate(QStringLiteral("desktop.lock-screen"), desktopGeneration);
    QCOMPARE(desktop.size(), 1);
    QCOMPARE(desktop.first().first().toString(), QStringLiteral("desktop.lock-screen"));
    QCOMPARE(application.size(), 0);

    // Once the application channel presents again, desktop ids are dead.
    access.publishTree(applicationTree());
    access.activate(QStringLiteral("desktop.lock-screen"));
    access.activate(QStringLiteral("desktop.lock-screen"), desktopGeneration);
    QCOMPARE(desktop.size(), 1);
    access.activate(QStringLiteral("fileNew"));
    QCOMPARE(application.size(), 1);
}

void GlobalMenuAppletAccessDesktopTests::inertDesktopTreeIsPaintedButNotActionable()
{
    GlobalMenuAppletAccess access;
    access.publishDesktopTree(desktopTree(), QStringLiteral("File Manager"), {});
    const QString generation = firstGeneration(access);
    QSignalSpy desktop(&access, &GlobalMenuAppletAccess::desktopActivationRequested);
    access.retainDesktopTreeInert();
    QVERIFY(access.desktopMenuShown());
    QVERIFY(!access.available());
    QCOMPARE(access.phase(), QStringLiteral("loading"));
    QCOMPARE(firstGeneration(access), generation);
    access.activate(QStringLiteral("desktop.lock-screen"), generation);
    access.activate(QStringLiteral("desktop.lock-screen"));
    QCOMPARE(desktop.size(), 0);
    // Re-proving the empty state restores the same delegates, actionable.
    access.publishDesktopTree(desktopTree(), QStringLiteral("File Manager"), {});
    QVERIFY(access.available());
    QCOMPARE(firstGeneration(access), generation);
    access.activate(QStringLiteral("desktop.lock-screen"), generation);
    QCOMPARE(desktop.size(), 1);
}

void GlobalMenuAppletAccessDesktopTests::identicalDesktopRepublishKeepsGeneration()
{
    GlobalMenuAppletAccess access;
    access.publishDesktopTree(desktopTree(), QStringLiteral("File Manager"), {});
    const QString generation = firstGeneration(access);
    QSignalSpy items(&access, &GlobalMenuAppletAccess::itemsChanged);
    access.publishDesktopTree(desktopTree(), QStringLiteral("File Manager"), {});
    QCOMPARE(items.size(), 0);
    QCOMPARE(firstGeneration(access), generation);
    access.publishDesktopTree(desktopTree("Lock"), QStringLiteral("File Manager"), {});
    QCOMPARE(items.size(), 1);
    QVERIFY(firstGeneration(access) != generation);
}

void GlobalMenuAppletAccessDesktopTests::invalidDesktopTreeWithdrawsFailClosed()
{
    GlobalMenuAppletAccess access;
    access.publishDesktopTree(desktopTree(), QStringLiteral("File Manager"), {});
    MenuTree hostile = desktopTree();
    hostile.items.append(hostile.items.first()); // duplicate ids
    access.publishDesktopTree(hostile, QStringLiteral("File Manager"), {});
    QVERIFY(!access.desktopMenuShown());
    QVERIFY(!access.available());
    QVERIFY(access.items().isEmpty());
}

void GlobalMenuAppletAccessDesktopTests::applicationFactsIgnoreThePresentedDesktop()
{
    GlobalMenuAppletAccess access;
    access.publishDesktopTree(desktopTree(), QStringLiteral("File Manager"), {});
    QVERIFY(access.available());
    QVERIFY(!access.applicationAvailable());
    QVERIFY(!access.applicationProjectionRetained());
    access.publishTree(applicationTree());
    QVERIFY(access.applicationAvailable());
    QVERIFY(access.applicationProjectionRetained());
    access.beginTransition();
    QVERIFY(!access.applicationAvailable());
    QVERIFY(access.applicationProjectionRetained());
}

void GlobalMenuAppletAccessDesktopTests::confirmationResolvesOnlyTheOutstandingTokenOnce()
{
    GlobalMenuAppletAccess access;
    QSignalSpy changed(&access, &GlobalMenuAppletAccess::confirmationChanged);
    QSignalSpy resolved(&access, &GlobalMenuAppletAccess::confirmationResolved);
    access.requestConfirmation(QStringLiteral("t1"), QStringLiteral("Log out?"),
                               QStringLiteral("Open applications will be asked to close."));
    QCOMPARE(changed.size(), 1);
    QCOMPARE(access.confirmation().value(QStringLiteral("title")).toString(),
             QStringLiteral("Log out?"));
    access.resolveConfirmation(QStringLiteral("stale"), true);
    access.resolveConfirmation(QString{}, true);
    QCOMPARE(resolved.size(), 0);
    access.resolveConfirmation(QStringLiteral("t1"), true);
    QCOMPARE(resolved.size(), 1);
    QCOMPARE(resolved.first().at(0).toString(), QStringLiteral("t1"));
    QCOMPARE(resolved.first().at(1).toBool(), true);
    QVERIFY(access.confirmation().isEmpty());
    access.resolveConfirmation(QStringLiteral("t1"), true);
    QCOMPARE(resolved.size(), 1);

    access.requestConfirmation(QStringLiteral("t2"), QStringLiteral("Restart?"), {});
    access.withdrawConfirmation(QStringLiteral("t2"));
    QVERIFY(access.confirmation().isEmpty());
    access.resolveConfirmation(QStringLiteral("t2"), true);
    QCOMPARE(resolved.size(), 1);
}

void GlobalMenuAppletAccessDesktopTests::newConfirmationDeclinesTheUnansweredOne()
{
    GlobalMenuAppletAccess access;
    QSignalSpy resolved(&access, &GlobalMenuAppletAccess::confirmationResolved);
    access.requestConfirmation(QStringLiteral("t1"), QStringLiteral("Restart?"), {});
    access.requestConfirmation(QStringLiteral("t2"), QStringLiteral("Shut down?"), {});
    QCOMPARE(resolved.size(), 1);
    QCOMPARE(resolved.first().at(0).toString(), QStringLiteral("t1"));
    QCOMPARE(resolved.first().at(1).toBool(), false);
    QCOMPARE(access.confirmation().value(QStringLiteral("token")).toString(),
             QStringLiteral("t2"));
}

QTEST_GUILESS_MAIN(GlobalMenuAppletAccessDesktopTests)

#include "tst_globalmenuappletaccess_desktop.moc"
