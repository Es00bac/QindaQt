// SPDX-License-Identifier: GPL-3.0-or-later

// ADR-0260: every desktop menu command reaches the ONE existing controller
// that owns it, through that controller's public boundary. The controllers
// are real; only their outermost seams (spawner, folder opener, workspace
// transport, session actions) are the shared desktop-controls test doubles.

#include "desktop_controls_test_support.h"
#include "desktopmenutargets.h"

#include "qindaqt/shell/clipboard_applet/clipboard_applet_controller.h"
#include "qindaqt/shell/desktop_controls/places_controller.h"
#include "qindaqt/shell/desktop_controls/system_menu_controller.h"
#include "qindaqt/shell/desktop_menu/desktop_menu_model.h"
#include "qindaqt/shell/desktop_menu/desktop_surface_commands.h"
#include "qindaqt/shell/global_menu/protocol/menu_validation.h"

#include <QFile>
#include <QSignalSpy>
#include <QtTest>

#include <memory>

using namespace QindaQt;
using namespace QindaQt::Shell::DesktopMenu;
using namespace QindaQt::Tests::DesktopControls;
using QindaQt::Shell::ShellDesktopMenuTargets;

namespace {

struct Routing {
    LauncherStack stack;
    std::unique_ptr<Shell::Launcher::LauncherAppletController> launcher;
    StubSessionActions session;
    std::unique_ptr<Shell::DesktopControls::SystemMenuController> systemMenu;
    RecordingFolderOpener opener;
    std::unique_ptr<Shell::DesktopControls::PlacesController> places;
    WorkspaceStack workspaces;
    ShellClipboardApplet::ClipboardAppletController clipboard{nullptr, true, false};
    DesktopSurfaceCommands surface;
    QList<QPair<QString, QString>> routes;
    int gatherToggles = 0;
    bool noteVisible = false;
    int noteToggles = 0;
    bool editingPanels = false;
    int panelToggles = 0;
    std::unique_ptr<ShellDesktopMenuTargets> targets;

    bool setUp()
    {
        // Distinct programs prove which desktop entry each command activated.
        if (!stack.root.isValid()
            || !stack.addEntry(QStringLiteral("org.qindaqt.Settings.desktop"),
                               QStringLiteral("System Settings"), QStringLiteral("/bin/true"))
            || !stack.addEntry(QStringLiteral("org.qindaqt.FileManager.desktop"),
                               QStringLiteral("QindaQt File Manager"),
                               QStringLiteral("/bin/false"))
            || !stack.addEntry(QStringLiteral("org.qindaqt.Welcome.desktop"),
                               QStringLiteral("Welcome to QindaQt"),
                               QStringLiteral("/bin/echo"))
            || !stack.scanner.start()) {
            return false;
        }
        launcher = std::make_unique<Shell::Launcher::LauncherAppletController>(
            &stack.scanner, nullptr, &stack.executor, true);
        session.canSuspend = true;
        systemMenu = std::make_unique<Shell::DesktopControls::SystemMenuController>(
            &session, launcher.get(), true,
            Shell::DesktopControls::SystemMenuController::defaultSettingsEntryId(),
            QStringLiteral("0.1.0"));
        places = std::make_unique<Shell::DesktopControls::PlacesController>(
            &opener, true,
            QList<Shell::DesktopControls::PlaceEntry>{
                {QStringLiteral("home"), QStringLiteral("Home"), QStringLiteral("/home/fixture"),
                 QStringLiteral("user-home")},
                {QStringLiteral("documents"), QStringLiteral("Documents"),
                 QStringLiteral("/home/fixture/Documents"), QStringLiteral("folder-documents")},
                {QStringLiteral("computer"), QStringLiteral("Computer"), QStringLiteral("/"),
                 QStringLiteral("drive-harddisk")}});
        workspaces.publishReady();
        surface.attachSurface(QStringLiteral("DP-1"), true);

        ShellDesktopMenuTargets::Hooks hooks;
        hooks.openSettingsRoute = [this](const QString &page, const QString &destination) {
            routes.append({page, destination});
            return true;
        };
        hooks.toggleGatherOverview = [this] { ++gatherToggles; };
        hooks.shortcutNoteVisible = [this] { return noteVisible; };
        hooks.toggleShortcutNote = [this] {
            ++noteToggles;
            noteVisible = !noteVisible;
        };
        hooks.editingPanels = [this] { return editingPanels; };
        hooks.toggleEditPanels = [this] {
            ++panelToggles;
            editingPanels = !editingPanels;
        };
        targets = std::make_unique<ShellDesktopMenuTargets>(
            ShellDesktopMenuTargets::Controllers{systemMenu.get(), places.get(),
                                                 &workspaces.controller, launcher.get(),
                                                 &clipboard, &surface},
            std::move(hooks));
        targets->setHostedApplets({.launcher = true, .clipboard = true});
        return true;
    }

    [[nodiscard]] QString lastProgram() const
    {
        return stack.spawner.requests.isEmpty() ? QString{}
                                                : stack.spawner.requests.constLast().program;
    }
};

bool perform(Routing &routing, const BuiltDesktopMenu &menu, const char *id)
{
    const QString key = QString::fromLatin1(id);
    if (!menu.commands.contains(key)) {
        qWarning("desktop menu has no entry %s", id);
        return false;
    }
    return routing.targets->perform(menu.commands.value(key));
}

} // namespace

class DesktopMenuTargetsTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void everyEntryIsPresentWhenEveryOwnerIs();
    void applicationAndSessionCommandsReachTheirOwners();
    void fileManagerAndPlaceCommandsReachTheirOwners();
    void desktopSurfaceCommandsReachTheSurfaces();
    void windowAndHelpCommandsReachTheirOwners();
    void staleWorkspaceSwitchFailsTheControllersFence();
    void absentOwnersAreOmittedOrDisabled();
    void ownerChangesNotifyTheMenu();
};

void DesktopMenuTargetsTest::everyEntryIsPresentWhenEveryOwnerIs()
{
    Routing routing;
    QVERIFY(routing.setUp());
    const DesktopMenuFacts facts = routing.targets->facts();
    for (const Capability &capability :
         {facts.aboutComputer, facts.systemSettings, facts.keyboardShortcuts, facts.lockScreen,
          facts.logOut, facts.suspend, facts.restart, facts.shutDown,
          facts.newFileManagerWindow, facts.find, facts.help, facts.newFolder, facts.selectAll,
          facts.cleanUp, facts.clipboardHistory, facts.showDesktop, facts.gatherOverview,
          facts.places, facts.workspaces, facts.shortcutNote, facts.editPanels}) {
        QVERIFY(capability.present);
        QVERIFY(capability.enabled);
    }
    // Paste stays disabled until the primary surface reports pasteable content.
    QVERIFY(facts.paste.present);
    QVERIFY(!facts.paste.enabled);
    QCOMPARE(facts.placeList.size(), 3);
    QCOMPARE(facts.workspaceList.size(), 3);
    QCOMPARE(facts.workspaceRevision, routing.workspaces.controller.revision());
    QVERIFY(facts.workspaceList.at(1).current);
    QVERIFY(QindaQt::Shell::GlobalMenu::Protocol::validateMenuTree(buildDesktopMenu(facts).tree)
                .accepted);
}

void DesktopMenuTargetsTest::applicationAndSessionCommandsReachTheirOwners()
{
    Routing routing;
    QVERIFY(routing.setUp());
    const BuiltDesktopMenu menu = buildDesktopMenu(routing.targets->facts());

    QVERIFY(perform(routing, menu, "desktop.about-computer"));
    QVERIFY(perform(routing, menu, "desktop.keyboard-shortcuts"));
    QCOMPARE(routing.routes,
             (QList<QPair<QString, QString>>{{QStringLiteral("about-computer"), QString{}},
                                             {QStringLiteral("input"),
                                              QStringLiteral("shortcuts")}}));
    QVERIFY(perform(routing, menu, "desktop.system-settings"));
    QCOMPARE(routing.lastProgram(), QStringLiteral("/bin/true"));

    QVERIFY(perform(routing, menu, "desktop.lock-screen"));
    QVERIFY(perform(routing, menu, "desktop.log-out"));
    QVERIFY(perform(routing, menu, "desktop.suspend"));
    QVERIFY(perform(routing, menu, "desktop.restart"));
    QVERIFY(perform(routing, menu, "desktop.shut-down"));
    QCOMPARE(routing.session.requests,
             (QStringList{QStringLiteral("lock"), QStringLiteral("logout"),
                          QStringLiteral("suspend"), QStringLiteral("reboot"),
                          QStringLiteral("poweroff")}));
}

void DesktopMenuTargetsTest::fileManagerAndPlaceCommandsReachTheirOwners()
{
    Routing routing;
    QVERIFY(routing.setUp());
    const BuiltDesktopMenu menu = buildDesktopMenu(routing.targets->facts());

    QVERIFY(perform(routing, menu, "file.new-window"));
    QCOMPARE(routing.lastProgram(), QStringLiteral("/bin/false"));
    QVERIFY(perform(routing, menu, "go.home"));
    QVERIFY(perform(routing, menu, "desktop.place.documents"));
    QVERIFY(perform(routing, menu, "desktop.place.computer"));
    QCOMPARE(routing.opener.opened,
             (QStringList{QStringLiteral("/home/fixture"),
                          QStringLiteral("/home/fixture/Documents"), QStringLiteral("/")}));

    QSignalSpy launcherOpen(routing.launcher.get(),
                            &Shell::Launcher::LauncherAppletController::openRequested);
    QVERIFY(perform(routing, menu, "desktop.find"));
    QCOMPARE(launcherOpen.size(), 1);
    QSignalSpy clipboardOpen(&routing.clipboard,
                             &ShellClipboardApplet::ClipboardAppletController::openRequested);
    QVERIFY(perform(routing, menu, "desktop.clipboard-history"));
    QCOMPARE(clipboardOpen.size(), 1);

    // A refused folder open is reported, never retried elsewhere.
    routing.opener.nextResult = {false, QStringLiteral("File Manager is not installed")};
    QVERIFY(!perform(routing, menu, "go.home"));
    QVERIFY(!routing.targets->lastFailure().isEmpty());
    QCOMPARE(routing.opener.opened.size(), 4);
}

void DesktopMenuTargetsTest::desktopSurfaceCommandsReachTheSurfaces()
{
    Routing routing;
    QVERIFY(routing.setUp());
    QSignalSpy requested(&routing.surface, &DesktopSurfaceCommands::commandRequested);
    routing.surface.reportPasteAvailable(QStringLiteral("DP-1"), true);
    const BuiltDesktopMenu menu = buildDesktopMenu(routing.targets->facts());
    QVERIFY(perform(routing, menu, "file.new-folder"));
    QVERIFY(perform(routing, menu, "edit.paste"));
    QVERIFY(perform(routing, menu, "edit.select-all"));
    QVERIFY(perform(routing, menu, "desktop.clean-up"));
    QCOMPARE(requested.size(), 4);
    QCOMPARE(requested.at(0).first().toString(), QStringLiteral("new-folder"));
    QCOMPARE(requested.at(1).first().toString(), QStringLiteral("paste"));
    QCOMPARE(requested.at(2).first().toString(), QStringLiteral("select-all"));
    QCOMPARE(requested.at(3).first().toString(), QStringLiteral("clean-up"));

    // A surface that went away meanwhile refuses instead of pretending.
    routing.surface.detachSurface(QStringLiteral("DP-1"));
    QVERIFY(!perform(routing, menu, "file.new-folder"));
    QCOMPARE(routing.targets->lastFailure(), QStringLiteral("Desktop icons are not shown"));
    QCOMPARE(requested.size(), 4);
}

void DesktopMenuTargetsTest::windowAndHelpCommandsReachTheirOwners()
{
    Routing routing;
    QVERIFY(routing.setUp());
    const BuiltDesktopMenu menu = buildDesktopMenu(routing.targets->facts());

    QVERIFY(perform(routing, menu, "desktop.workspace.ws-1"));
    QCOMPARE(routing.workspaces.transport.switchRequests.size(), 1);
    QCOMPARE(routing.workspaces.transport.switchRequests.constLast().desktopId,
             QStringLiteral("ws-1"));
    routing.workspaces.transport.finishSwitch(
        routing.workspaces.transport.switchRequests.constLast(), true);

    const BuiltDesktopMenu current = buildDesktopMenu(routing.targets->facts());
    QVERIFY(perform(routing, current, "desktop.show-desktop"));
    QCOMPARE(routing.workspaces.transport.showDesktopRequests.size(), 1);
    QVERIFY(routing.workspaces.transport.showDesktopRequests.constLast().showing);

    QVERIFY(perform(routing, current, "desktop.gather-overview"));
    QCOMPARE(routing.gatherToggles, 1);
    QVERIFY(perform(routing, current, "desktop.shortcut-note"));
    QCOMPARE(routing.noteToggles, 1);
    QVERIFY(routing.targets->facts().shortcutNoteVisible);
    QVERIFY(!routing.targets->facts().editingPanels);
    QVERIFY(perform(routing, current, "desktop.edit-panels"));
    QCOMPARE(routing.panelToggles, 1);
    QVERIFY(routing.targets->facts().editingPanels);
    QVERIFY(perform(routing, current, "desktop.help"));
    QCOMPARE(routing.lastProgram(), QStringLiteral("/bin/echo"));
    QVERIFY(QFile::exists(
        QStringLiteral(QINDAQT_SOURCE_DIR "/src/apps/welcome/org.qindaqt.Welcome.desktop")));
    QCOMPARE(ShellDesktopMenuTargets::helpDesktopEntryId(), QStringLiteral("org.qindaqt.Welcome"));
}

void DesktopMenuTargetsTest::staleWorkspaceSwitchFailsTheControllersFence()
{
    Routing routing;
    QVERIFY(routing.setUp());
    const BuiltDesktopMenu stale = buildDesktopMenu(routing.targets->facts());
    // The workspace list changes after the menu was built.
    routing.workspaces.transport.change(QStringLiteral(":1.7"));
    QTRY_VERIFY(routing.workspaces.transport.snapshotRequests.size() >= 2);
    routing.workspaces.transport.reply(routing.workspaces.transport.snapshotRequests.constLast(),
                                       Tests::Workspaces::fixtureSnapshot(QStringLiteral("ws-3")));
    QTRY_VERIFY(routing.workspaces.controller.revision()
                != stale.commands.value(QStringLiteral("desktop.workspace.ws-1")).revision);
    QVERIFY(!perform(routing, stale, "desktop.workspace.ws-1"));
    QVERIFY(routing.workspaces.transport.switchRequests.isEmpty());
    QCOMPARE(routing.targets->lastFailure(),
             QStringLiteral("The workspace list changed; try again"));
}

void DesktopMenuTargetsTest::absentOwnersAreOmittedOrDisabled()
{
    ShellDesktopMenuTargets empty({}, {});
    QCOMPARE(empty.facts(), DesktopMenuFacts{});
    QVERIFY(buildDesktopMenu(empty.facts()).tree.items.isEmpty());
    for (const DesktopCommand kind :
         {DesktopCommand::AboutComputer, DesktopCommand::LockScreen, DesktopCommand::NewFolder,
          DesktopCommand::Find, DesktopCommand::ClipboardHistory, DesktopCommand::ShowDesktop,
          DesktopCommand::GatherOverview, DesktopCommand::ShortcutNote, DesktopCommand::Help,
          DesktopCommand::EditPanels}) {
        QVERIFY(!empty.perform({kind, {}, 0}));
        QVERIFY(!empty.lastFailure().isEmpty());
    }

    Routing routing;
    QVERIFY(routing.setUp());
    // Request-only popups need a renderer in the adopted layout.
    routing.targets->setHostedApplets({});
    DesktopMenuFacts facts = routing.targets->facts();
    QVERIFY(!facts.find.present);
    QVERIFY(!facts.clipboardHistory.present);
    QVERIFY(!routing.targets->perform({DesktopCommand::Find, {}, 0}));
    // Desktop icons follow an attached primary surface.
    routing.surface.detachSurface(QStringLiteral("DP-1"));
    facts = routing.targets->facts();
    QVERIFY(!facts.newFolder.present && !facts.paste.present && !facts.selectAll.present
            && !facts.cleanUp.present);
    // Session admission disables (never hides) its entries; a pending request
    // disables all of them, exactly as the system menu applet does.
    routing.session.canLogout = false;
    facts = routing.targets->facts();
    QVERIFY(facts.logOut.present && !facts.logOut.enabled);
    QVERIFY(facts.lockScreen.enabled);
    routing.session.pending = true;
    facts = routing.targets->facts();
    QVERIFY(!facts.lockScreen.enabled && !facts.shutDown.enabled);
}

void DesktopMenuTargetsTest::ownerChangesNotifyTheMenu()
{
    Routing routing;
    QVERIFY(routing.setUp());
    QSignalSpy changed(routing.targets.get(), &DesktopMenuTargets::factsChanged);
    Q_EMIT routing.session.availabilityChanged();
    Q_EMIT routing.session.pendingChanged();
    QCOMPARE(changed.size(), 2);
    routing.surface.reportPasteAvailable(QStringLiteral("DP-1"), true);
    QCOMPARE(changed.size(), 3);
    // A workspace change is a reread; the controller reports the new
    // snapshot, not the change hint itself.
    routing.workspaces.transport.change(QStringLiteral(":1.7"));
    QTRY_VERIFY(routing.workspaces.transport.snapshotRequests.size() >= 2);
    routing.workspaces.transport.reply(routing.workspaces.transport.snapshotRequests.constLast(),
                                       Tests::Workspaces::fixtureSnapshot(QStringLiteral("ws-3")));
    QTRY_VERIFY(changed.size() >= 4);
    const qsizetype before = changed.size();
    routing.targets->notifyFactsChanged();
    QCOMPARE(changed.size(), before + 1);
    routing.targets->setHostedApplets({.launcher = true, .clipboard = true});
    QCOMPARE(changed.size(), before + 1);
}

QTEST_GUILESS_MAIN(DesktopMenuTargetsTest)

#include "tst_desktop_menu_targets.moc"
