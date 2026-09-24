// SPDX-License-Identifier: GPL-3.0-or-later

// ADR-0260: desktop menu selection and activation against the REAL global
// menu facade. Presence stands in for the compositor-authenticated identity
// (the runtime row drives it from the real identity client).

#include "desktop_menu_test_support.h"

#include "qindaqt/shell/desktop_menu/desktop_menu_controller.h"

#include <qindaqt/shell/global_menu/applet/globalmenuappletaccess.h>

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Shell::DesktopMenu;
using namespace QindaQt::Tests::DesktopMenu;
using QindaQt::Shell::GlobalMenu::GlobalMenuAppletAccess;
using Presence = DesktopMenuController::Presence;

namespace {

QindaQt::Shell::GlobalMenu::Protocol::MenuTree applicationTree()
{
    using namespace QindaQt::Shell::GlobalMenu::Protocol;
    MenuItem open;
    open.id = QStringLiteral("7");
    open.kind = MenuItemKind::Action;
    open.text = QStringLiteral("Open");
    MenuItem file;
    file.id = QStringLiteral("3");
    file.kind = MenuItemKind::Submenu;
    file.text = QStringLiteral("File");
    file.children = {open};
    MenuTree tree;
    tree.ownerWindowId = QUuid::createUuid();
    tree.epoch = QUuid::createUuid();
    tree.revision = 1;
    tree.items = {file};
    return tree;
}

QString topText(const GlobalMenuAppletAccess &access)
{
    return access.items().value(0).toMap().value(QStringLiteral("text")).toString();
}

QString generation(const GlobalMenuAppletAccess &access)
{
    return access.items().value(0).toMap().value(QStringLiteral("generation")).toString();
}

struct Fixture {
    GlobalMenuAppletAccess access;
    RecordingTargets targets;
    DesktopMenuController controller{access, targets};
    QList<QPair<DesktopMenuCommand, bool>> outcomes;

    Fixture()
    {
        QObject::connect(&controller, &DesktopMenuController::commandPerformed, &controller,
                         [this](const DesktopMenuCommand &command, bool accepted) {
                             outcomes.append({command, accepted});
                         });
    }

    void showDesktop()
    {
        controller.setEnabled(true);
        controller.setPresence(Presence::NoApplication);
    }
};

} // namespace

class DesktopMenuControllerTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void noActiveApplicationShowsTheDesktopMenu();
    void layoutsWithoutAGlobalMenuNeverShowIt();
    void activeApplicationReplacesItImmediately();
    void menuLessApplicationRetiresTheInertMenuAfterGrace();
    void focusHandBackRestoresTheDesktopMenu();
    void identityRereadKeepsAnOpenMenuActionable();
    void unprovenIdentityWithdrawsAfterGrace();
    void activationRoutesExactlyOneCommand();
    void refusedCommandReportsTheOwnersFailure();
    void factChangesRepublishWithoutLosingIdentity();
    void sessionEndingCommandsRunOnlyAfterConfirmation();
    void confirmationIsRecheckedWhenAnswered();
    void disablingWithdrawsMenuAndQuestion();
};

void DesktopMenuControllerTest::noActiveApplicationShowsTheDesktopMenu()
{
    Fixture f;
    f.showDesktop();
    QVERIFY(f.controller.published());
    QVERIFY(f.controller.actionable());
    QVERIFY(f.access.desktopMenuShown());
    QVERIFY(f.access.available());
    QCOMPARE(topText(f.access), QStringLiteral("File Manager"));
    QCOMPARE(f.access.desktopMenuTitle(), QStringLiteral("File Manager"));
    QCOMPARE(f.access.desktopMenuIconName(), QStringLiteral("org.qindaqt.FileManager"));
}

void DesktopMenuControllerTest::layoutsWithoutAGlobalMenuNeverShowIt()
{
    Fixture f;
    f.controller.setPresence(Presence::NoApplication);
    QVERIFY(!f.controller.published());
    QVERIFY(f.access.items().isEmpty());
    f.controller.setEnabled(true);
    QVERIFY(f.access.desktopMenuShown());
    f.controller.setEnabled(false);
    QVERIFY(!f.access.desktopMenuShown());
    QVERIFY(f.access.items().isEmpty());
    QVERIFY(!f.access.available());
}

void DesktopMenuControllerTest::activeApplicationReplacesItImmediately()
{
    Fixture f;
    f.showDesktop();
    const QString desktopGeneration = generation(f.access);
    QSignalSpy desktop(&f.access, &GlobalMenuAppletAccess::desktopActivationRequested);

    f.controller.setPresence(Presence::ApplicationActive);
    // Inert at once: still painted (no panel flash) but never actionable for
    // the new focus.
    QVERIFY(f.access.desktopMenuShown());
    QVERIFY(!f.access.available());
    QCOMPARE(f.access.phase(), QStringLiteral("loading"));
    f.access.activate(QStringLiteral("desktop.lock-screen"), desktopGeneration);
    QCOMPARE(desktop.size(), 0);

    // The application's first tree replaces it the moment it lands.
    f.access.publishTree(applicationTree());
    QVERIFY(!f.access.desktopMenuShown());
    QCOMPARE(topText(f.access), QStringLiteral("File"));
    QVERIFY(f.access.available());
    QTRY_VERIFY_WITH_TIMEOUT(!f.controller.published(), 2'000);
    QCOMPARE(topText(f.access), QStringLiteral("File"));
    QVERIFY(f.targets.performed.isEmpty());
}

void DesktopMenuControllerTest::menuLessApplicationRetiresTheInertMenuAfterGrace()
{
    Fixture f;
    f.showDesktop();
    f.controller.setPresence(Presence::ApplicationActive);
    QVERIFY(f.access.desktopMenuShown());
    QTest::qWait(DesktopMenuController::kRetireGraceMilliseconds / 2);
    // Repeated observations never extend the grace.
    f.controller.setPresence(Presence::ApplicationActive);
    f.controller.setPresence(Presence::Unknown);
    QTRY_VERIFY_WITH_TIMEOUT(!f.access.desktopMenuShown(), 2'000);
    QVERIFY(f.access.items().isEmpty());
    QCOMPARE(f.access.phase(), QStringLiteral("unavailable"));
}

void DesktopMenuControllerTest::focusHandBackRestoresTheDesktopMenu()
{
    Fixture f;
    f.showDesktop();
    f.controller.setPresence(Presence::ApplicationActive);
    f.access.publishTree(applicationTree());
    QTRY_VERIFY_WITH_TIMEOUT(!f.controller.published(), 2'000);

    // Focus returns to the desktop: the desktop menu is actionable again at
    // once, and is presented as soon as the application's retained tree goes.
    f.controller.setPresence(Presence::NoApplication);
    QVERIFY(f.controller.actionable());
    QCOMPARE(topText(f.access), QStringLiteral("File"));
    f.access.publishUnavailable();
    QVERIFY(f.access.desktopMenuShown());
    QVERIFY(f.access.available());
    QSignalSpy desktop(&f.access, &GlobalMenuAppletAccess::desktopActivationRequested);
    f.access.activate(QStringLiteral("desktop.gather-overview"), generation(f.access));
    QCOMPARE(desktop.size(), 1);
    QCOMPARE(f.targets.performed.size(), 1);
    QCOMPARE(f.targets.performed.first().kind, DesktopCommand::GatherOverview);
}

void DesktopMenuControllerTest::identityRereadKeepsAnOpenMenuActionable()
{
    Fixture f;
    f.showDesktop();
    const QString before = generation(f.access);
    QSignalSpy items(&f.access, &GlobalMenuAppletAccess::itemsChanged);
    // Opening the menu's own popup invalidates the identity; the reread
    // proves the empty state again well inside the grace.
    f.controller.setPresence(Presence::Unknown);
    QVERIFY(f.access.available());
    f.access.activate(QStringLiteral("desktop.suspend"), before);
    QCOMPARE(f.targets.performed.size(), 1);
    f.controller.setPresence(Presence::NoApplication);
    QTest::qWait(DesktopMenuController::kRetireGraceMilliseconds + 150);
    QVERIFY(f.access.desktopMenuShown());
    QVERIFY(f.access.available());
    QCOMPARE(generation(f.access), before);
    QCOMPARE(items.size(), 0);
}

void DesktopMenuControllerTest::unprovenIdentityWithdrawsAfterGrace()
{
    Fixture f;
    f.showDesktop();
    f.controller.setPresence(Presence::Unknown);
    QTRY_VERIFY_WITH_TIMEOUT(!f.access.desktopMenuShown(), 2'000);
    QVERIFY(!f.controller.published());
    // The next proof of the empty state shows it again.
    f.controller.setPresence(Presence::NoApplication);
    QVERIFY(f.access.desktopMenuShown());
}

void DesktopMenuControllerTest::activationRoutesExactlyOneCommand()
{
    Fixture f;
    f.showDesktop();
    const QString current = generation(f.access);
    f.access.activate(QStringLiteral("desktop.place.documents"), current);
    f.access.activate(QStringLiteral("desktop.workspace.ws-2"), current);
    f.access.activate(QStringLiteral("edit.select-all"), current);
    // Unknown, submenu, and separator ids never become commands.
    f.access.activate(QStringLiteral("desktop.menu.go"), current);
    f.access.activate(QStringLiteral("desktop.menu.go.separator-1"), current);
    f.access.activate(QStringLiteral("7"), current);
    QCOMPARE(f.targets.performed.size(), 3);
    QCOMPARE(f.targets.performed.at(0),
             (DesktopMenuCommand{DesktopCommand::OpenPlace, QStringLiteral("documents"), 0}));
    QCOMPARE(f.targets.performed.at(1),
             (DesktopMenuCommand{DesktopCommand::SwitchWorkspace, QStringLiteral("ws-2"), 7}));
    QCOMPARE(f.targets.performed.at(2),
             (DesktopMenuCommand{DesktopCommand::SelectAll, {}, 0}));
    QCOMPARE(f.outcomes.size(), 3);
    QVERIFY(f.outcomes.at(0).second);
}

void DesktopMenuControllerTest::refusedCommandReportsTheOwnersFailure()
{
    Fixture f;
    f.showDesktop();
    f.targets.accept = false;
    f.access.activate(QStringLiteral("desktop.help"), generation(f.access));
    QCOMPARE(f.outcomes.size(), 1);
    QVERIFY(!f.outcomes.first().second);
    QCOMPARE(f.controller.lastFailure(), QStringLiteral("owner-refused"));
}

void DesktopMenuControllerTest::factChangesRepublishWithoutLosingIdentity()
{
    Fixture f;
    f.showDesktop();
    QSignalSpy items(&f.access, &GlobalMenuAppletAccess::itemsChanged);
    // An unchanged fact set republishes nothing.
    f.targets.change(f.targets.current);
    QCOMPARE(items.size(), 0);
    DesktopMenuFacts facts = f.targets.current;
    facts.workspaceList.append({QStringLiteral("ws-3"), QStringLiteral("Work"), false});
    facts.workspaceRevision = 8;
    f.targets.change(facts);
    QCOMPARE(items.size(), 1);
    f.access.activate(QStringLiteral("desktop.workspace.ws-3"), generation(f.access));
    QCOMPARE(f.targets.performed.size(), 1);
    QCOMPARE(f.targets.performed.first().revision, quint64{8});

    // While an application's menu is shown, fact changes stay off the bar.
    f.controller.setPresence(Presence::ApplicationActive);
    f.access.publishTree(applicationTree());
    facts.workspaceList.removeLast();
    f.targets.change(facts);
    QCOMPARE(topText(f.access), QStringLiteral("File"));
}

void DesktopMenuControllerTest::sessionEndingCommandsRunOnlyAfterConfirmation()
{
    Fixture f;
    f.showDesktop();
    f.access.activate(QStringLiteral("desktop.shut-down"), generation(f.access));
    QVERIFY(f.targets.performed.isEmpty());
    const QVariantMap question = f.access.confirmation();
    QCOMPARE(question.value(QStringLiteral("title")).toString(), QStringLiteral("Shut down?"));
    QCOMPARE(question.value(QStringLiteral("text")).toString(),
             QStringLiteral("The computer will shut down now."));
    const QString token = question.value(QStringLiteral("token")).toString();
    f.access.resolveConfirmation(token, false);
    QVERIFY(f.targets.performed.isEmpty());

    f.access.activate(QStringLiteral("desktop.log-out"), generation(f.access));
    const QString logout = f.access.confirmation().value(QStringLiteral("token")).toString();
    QVERIFY(logout != token);
    // Asking again declines the unanswered question instead of stacking it.
    f.access.activate(QStringLiteral("desktop.restart"), generation(f.access));
    f.access.resolveConfirmation(logout, true);
    QVERIFY(f.targets.performed.isEmpty());
    const QString restart = f.access.confirmation().value(QStringLiteral("token")).toString();
    f.access.resolveConfirmation(restart, true);
    QCOMPARE(f.targets.performed.size(), 1);
    QCOMPARE(f.targets.performed.first().kind, DesktopCommand::Restart);
    // Lock and suspend never ask.
    f.access.activate(QStringLiteral("desktop.lock-screen"), generation(f.access));
    QCOMPARE(f.targets.performed.size(), 2);
    QVERIFY(f.access.confirmation().isEmpty());
}

void DesktopMenuControllerTest::confirmationIsRecheckedWhenAnswered()
{
    Fixture f;
    f.showDesktop();
    f.access.activate(QStringLiteral("desktop.log-out"), generation(f.access));
    const QString token = f.access.confirmation().value(QStringLiteral("token")).toString();
    DesktopMenuFacts facts = f.targets.current;
    facts.logOut = Capability::disabled();
    f.targets.change(facts);
    f.access.resolveConfirmation(token, true);
    QVERIFY(f.targets.performed.isEmpty());
    QCOMPARE(f.outcomes.size(), 1);
    QVERIFY(!f.outcomes.first().second);
    QCOMPARE(f.controller.lastFailure(), QStringLiteral("no-longer-available"));
}

void DesktopMenuControllerTest::disablingWithdrawsMenuAndQuestion()
{
    Fixture f;
    f.showDesktop();
    f.access.activate(QStringLiteral("desktop.restart"), generation(f.access));
    const QString token = f.access.confirmation().value(QStringLiteral("token")).toString();
    QVERIFY(!token.isEmpty());
    f.controller.setEnabled(false);
    QVERIFY(f.access.confirmation().isEmpty());
    QVERIFY(!f.access.desktopMenuShown());
    f.access.resolveConfirmation(token, true);
    QVERIFY(f.targets.performed.isEmpty());
}

QTEST_GUILESS_MAIN(DesktopMenuControllerTest)

#include "tst_desktop_menu_controller.moc"
