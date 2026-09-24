// SPDX-License-Identifier: GPL-3.0-or-later

// ADR-0260: the pure desktop menu builder. Every tree must validate under
// the canonical bounds, match File Manager's public catalog word for word,
// omit absent capabilities, disable refused ones, and map each id to exactly
// one command.

#include "desktop_menu_test_support.h"

#include "file_manager_menu_catalog.h"
#include "qindaqt/shell/desktop_menu/desktop_menu_model.h"

#include <qindaqt/shell/global_menu/protocol/menu_limits.h>
#include <qindaqt/shell/global_menu/protocol/menu_validation.h>

#include <QtTest>

using namespace QindaQt::Shell::DesktopMenu;
using namespace QindaQt::Tests::DesktopMenu;
using QindaQt::Shell::GlobalMenu::Protocol::MenuItem;
using QindaQt::Shell::GlobalMenu::Protocol::MenuItemKind;
namespace Catalog = QindaQt::Apps::FileManager::MenuCatalog;

namespace {

QStringList texts(const QList<MenuItem> &items)
{
    QStringList result;
    for (const MenuItem &item : items) {
        result.append(item.kind == MenuItemKind::Separator ? QStringLiteral("—") : item.text);
    }
    return result;
}

const MenuItem &topLevel(const BuiltDesktopMenu &menu, const char *id)
{
    static const MenuItem missing;
    for (const MenuItem &item : menu.tree.items) {
        if (item.id == QLatin1StringView(id)) {
            return item;
        }
    }
    return missing;
}

void verifySeparatorsOnlyBetweenGroups(const QList<MenuItem> &items)
{
    for (const MenuItem &item : items) {
        if (item.kind != MenuItemKind::Submenu) {
            continue;
        }
        const QList<MenuItem> &children = item.children;
        QVERIFY2(!children.isEmpty(), qPrintable(item.id));
        QVERIFY(children.first().kind != MenuItemKind::Separator);
        QVERIFY(children.last().kind != MenuItemKind::Separator);
        for (qsizetype i = 1; i < children.size(); ++i) {
            QVERIFY(!(children.at(i).kind == MenuItemKind::Separator
                      && children.at(i - 1).kind == MenuItemKind::Separator));
        }
    }
}

} // namespace

class DesktopMenuModelTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void fullMenuValidatesAndReadsLikeFinder();
    void labelsAndTitlesComeFromTheFileManagerCatalog();
    void desktopEntriesAdvertiseNoUnhonouredShortcut();
    void everyActionMapsToExactlyOneCommand();
    void absentCapabilitiesAreOmittedWithTheirMenus();
    void refusedCapabilitiesAreDisabledNotHidden();
    void desktopIconsEntriesFollowTheSurface();
    void hostileWorkspacesStillValidate();
    void titleAndIconComeFromTheCatalog();
    void sessionEndingCommandsAskFirst();
};

void DesktopMenuModelTest::fullMenuValidatesAndReadsLikeFinder()
{
    const BuiltDesktopMenu menu = buildDesktopMenu(fullFacts());
    const auto validation =
        QindaQt::Shell::GlobalMenu::Protocol::validateMenuTree(menu.tree);
    QVERIFY2(validation.accepted, qPrintable(validation.reasonCode + validation.path));
    QCOMPARE(texts(menu.tree.items),
             (QStringList{QStringLiteral("File Manager"), QStringLiteral("File"),
                          QStringLiteral("Edit"), QStringLiteral("View"),
                          QStringLiteral("Go"), QStringLiteral("Window"),
                          QStringLiteral("Help")}));
    QCOMPARE(texts(topLevel(menu, "desktop.menu.application").children),
             (QStringList{QStringLiteral("About This Computer"), QStringLiteral("—"),
                          QStringLiteral("System Settings…"),
                          QStringLiteral("Keyboard Shortcuts…"), QStringLiteral("—"),
                          QStringLiteral("Lock Screen"), QStringLiteral("Log Out…"),
                          QStringLiteral("—"), QStringLiteral("Suspend"),
                          QStringLiteral("Restart…"), QStringLiteral("Shut Down…")}));
    QCOMPARE(texts(topLevel(menu, "desktop.menu.file").children),
             (QStringList{QStringLiteral("New File Manager Window"),
                          QStringLiteral("New Folder"), QStringLiteral("—"),
                          QStringLiteral("Find…")}));
    QCOMPARE(texts(topLevel(menu, "desktop.menu.edit").children),
             (QStringList{QStringLiteral("Paste"), QStringLiteral("Select All"),
                          QStringLiteral("—"), QStringLiteral("Show Clipboard History")}));
    QCOMPARE(texts(topLevel(menu, "desktop.menu.view").children),
             (QStringList{QStringLiteral("Show Desktop"), QStringLiteral("Gather Overview"),
                          QStringLiteral("—"), QStringLiteral("Clean Up"),
                          QStringLiteral("—"), QStringLiteral("Edit Panels")}));
    QCOMPARE(texts(topLevel(menu, "desktop.menu.go").children),
             (QStringList{QStringLiteral("Home Folder"), QStringLiteral("Desktop"),
                          QStringLiteral("Documents"), QStringLiteral("Downloads"),
                          QStringLiteral("Music"), QStringLiteral("Pictures"),
                          QStringLiteral("Videos"), QStringLiteral("—"),
                          QStringLiteral("Computer")}));
    QCOMPARE(texts(topLevel(menu, "desktop.menu.window").children),
             (QStringList{QStringLiteral("Main"), QStringLiteral("Games")}));
    QCOMPARE(texts(topLevel(menu, "desktop.menu.help").children),
             (QStringList{QStringLiteral("QindaQt Help"), QStringLiteral("Keyboard Shortcuts")}));
    verifySeparatorsOnlyBetweenGroups(menu.tree.items);
    QVERIFY(menu.tree.ownerWindowId.isNull());
    QVERIFY(menu.tree.epoch.isNull());

    const MenuItem *main = findItem(menu.tree.items, QStringLiteral("desktop.workspace.ws-1"));
    QVERIFY(main != nullptr);
    QVERIFY(main->checkable);
    QVERIFY(main->checked);
    QCOMPARE(main->radioGroup, QStringLiteral("desktop.workspaces"));

    // View > Edit Panels (ADR-0266) is checked exactly while panels are edited.
    const MenuItem *editPanels = findItem(menu.tree.items, QStringLiteral("desktop.edit-panels"));
    QVERIFY(editPanels != nullptr && editPanels->checkable && !editPanels->checked);
    DesktopMenuFacts editing = fullFacts();
    editing.editingPanels = true;
    const BuiltDesktopMenu editingMenu = buildDesktopMenu(editing);
    editPanels = findItem(editingMenu.tree.items, QStringLiteral("desktop.edit-panels"));
    QVERIFY(editPanels != nullptr && editPanels->checked);
}

void DesktopMenuModelTest::labelsAndTitlesComeFromTheFileManagerCatalog()
{
    const BuiltDesktopMenu menu = buildDesktopMenu(fullFacts());
    // Menu titles are the catalog's own.
    for (const Catalog::MenuDefinition &definition : Catalog::menus()) {
        const MenuItem *submenu =
            findItem(menu.tree.items, QStringLiteral("desktop.menu.") + definition.id);
        QVERIFY2(submenu != nullptr, qPrintable(definition.id));
        QCOMPARE(submenu->text, definition.label);
    }
    // Every entry whose id is a File Manager action reads exactly as it does in
    // a File Manager window; none is spelled by the desktop menu itself.
    int catalogBacked = 0;
    for (auto it = menu.commands.cbegin(); it != menu.commands.cend(); ++it) {
        const std::optional<Catalog::ActionDefinition> definition = Catalog::findAction(it.key());
        if (!definition) {
            QVERIFY2(it.key().startsWith(QLatin1StringView("desktop.")), qPrintable(it.key()));
            continue;
        }
        const MenuItem *item = findItem(menu.tree.items, it.key());
        QVERIFY(item != nullptr);
        QCOMPARE(item->text, definition->label);
        ++catalogBacked;
    }
    // file.new-window, file.new-folder, edit.paste, edit.select-all, go.home
    QCOMPARE(catalogBacked, 5);
    QCOMPARE(Catalog::findAction(QStringLiteral("file.new-folder"))->label,
             QStringLiteral("New Folder"));
    QCOMPARE(Catalog::findAction(QStringLiteral("go.home"))->label,
             QStringLiteral("Home Folder"));
}

void DesktopMenuModelTest::desktopEntriesAdvertiseNoUnhonouredShortcut()
{
    // AGENT-NOTE (ADR-0260): the desktop surface is a focus-less layer, so no
    // File Manager window shortcut fires there. The catalog still carries the
    // shortcut (proved equal by the File Manager catalog row); the desktop
    // menu shows none rather than a key that does nothing.
    const BuiltDesktopMenu menu = buildDesktopMenu(fullFacts());
    for (auto it = menu.commands.cbegin(); it != menu.commands.cend(); ++it) {
        const MenuItem *item = findItem(menu.tree.items, it.key());
        QVERIFY(item != nullptr);
        QVERIFY2(item->shortcutText.isEmpty(), qPrintable(it.key()));
        QCOMPARE(item->mnemonicIndex, -1);
    }
    QVERIFY(!Catalog::findAction(QStringLiteral("edit.select-all"))->shortcut.isEmpty());
}

void DesktopMenuModelTest::everyActionMapsToExactlyOneCommand()
{
    const BuiltDesktopMenu menu = buildDesktopMenu(fullFacts());
    const QHash<QString, DesktopMenuCommand> expected{
        {QStringLiteral("desktop.about-computer"), {DesktopCommand::AboutComputer, {}, 0}},
        {QStringLiteral("desktop.system-settings"), {DesktopCommand::SystemSettings, {}, 0}},
        {QStringLiteral("desktop.keyboard-shortcuts"), {DesktopCommand::KeyboardShortcuts, {}, 0}},
        {QStringLiteral("desktop.lock-screen"), {DesktopCommand::LockScreen, {}, 0}},
        {QStringLiteral("desktop.log-out"), {DesktopCommand::LogOut, {}, 0}},
        {QStringLiteral("desktop.suspend"), {DesktopCommand::Suspend, {}, 0}},
        {QStringLiteral("desktop.restart"), {DesktopCommand::Restart, {}, 0}},
        {QStringLiteral("desktop.shut-down"), {DesktopCommand::ShutDown, {}, 0}},
        {QStringLiteral("file.new-window"), {DesktopCommand::NewFileManagerWindow, {}, 0}},
        {QStringLiteral("file.new-folder"), {DesktopCommand::NewFolder, {}, 0}},
        {QStringLiteral("desktop.find"), {DesktopCommand::Find, {}, 0}},
        {QStringLiteral("edit.paste"), {DesktopCommand::Paste, {}, 0}},
        {QStringLiteral("edit.select-all"), {DesktopCommand::SelectAll, {}, 0}},
        {QStringLiteral("desktop.clipboard-history"), {DesktopCommand::ClipboardHistory, {}, 0}},
        {QStringLiteral("desktop.show-desktop"), {DesktopCommand::ShowDesktop, {}, 0}},
        {QStringLiteral("desktop.gather-overview"), {DesktopCommand::GatherOverview, {}, 0}},
        {QStringLiteral("desktop.clean-up"), {DesktopCommand::CleanUp, {}, 0}},
        {QStringLiteral("go.home"), {DesktopCommand::OpenPlace, QStringLiteral("home"), 0}},
        {QStringLiteral("desktop.place.desktop"),
         {DesktopCommand::OpenPlace, QStringLiteral("desktop"), 0}},
        {QStringLiteral("desktop.place.documents"),
         {DesktopCommand::OpenPlace, QStringLiteral("documents"), 0}},
        {QStringLiteral("desktop.place.downloads"),
         {DesktopCommand::OpenPlace, QStringLiteral("downloads"), 0}},
        {QStringLiteral("desktop.place.music"),
         {DesktopCommand::OpenPlace, QStringLiteral("music"), 0}},
        {QStringLiteral("desktop.place.pictures"),
         {DesktopCommand::OpenPlace, QStringLiteral("pictures"), 0}},
        {QStringLiteral("desktop.place.videos"),
         {DesktopCommand::OpenPlace, QStringLiteral("videos"), 0}},
        {QStringLiteral("desktop.place.computer"),
         {DesktopCommand::OpenPlace, QStringLiteral("computer"), 0}},
        {QStringLiteral("desktop.workspace.ws-1"),
         {DesktopCommand::SwitchWorkspace, QStringLiteral("ws-1"), 7}},
        {QStringLiteral("desktop.workspace.ws-2"),
         {DesktopCommand::SwitchWorkspace, QStringLiteral("ws-2"), 7}},
        {QStringLiteral("desktop.help"), {DesktopCommand::Help, {}, 0}},
        {QStringLiteral("desktop.shortcut-note"), {DesktopCommand::ShortcutNote, {}, 0}},
        {QStringLiteral("desktop.edit-panels"), {DesktopCommand::EditPanels, {}, 0}},
    };
    QCOMPARE(menu.commands, expected);
    // Every action in the tree has a command and nothing else does.
    for (auto it = expected.cbegin(); it != expected.cend(); ++it) {
        const MenuItem *item = findItem(menu.tree.items, it.key());
        QVERIFY2(item != nullptr && item->kind == MenuItemKind::Action, qPrintable(it.key()));
    }
}

void DesktopMenuModelTest::absentCapabilitiesAreOmittedWithTheirMenus()
{
    // With nothing present the menu is empty (the facade then presents the
    // application channel's own unavailable truth, never a hollow bar).
    QVERIFY(buildDesktopMenu(DesktopMenuFacts{}).tree.items.isEmpty());

    DesktopMenuFacts facts = fullFacts();
    facts.aboutComputer = {};
    facts.keyboardShortcuts = {};
    facts.lockScreen = {};
    facts.logOut = {};
    facts.suspend = {};
    facts.restart = {};
    facts.shutDown = {};
    facts.workspaces = {};
    facts.help = {};
    facts.shortcutNote = {};
    const BuiltDesktopMenu menu = buildDesktopMenu(facts);
    QVERIFY(QindaQt::Shell::GlobalMenu::Protocol::validateMenuTree(menu.tree).accepted);
    QCOMPARE(texts(topLevel(menu, "desktop.menu.application").children),
             QStringList{QStringLiteral("System Settings…")});
    QVERIFY(findItem(menu.tree.items, QStringLiteral("desktop.menu.window")) == nullptr);
    QVERIFY(findItem(menu.tree.items, QStringLiteral("desktop.menu.help")) == nullptr);
    QVERIFY(!menu.commands.contains(QStringLiteral("desktop.log-out")));
    verifySeparatorsOnlyBetweenGroups(menu.tree.items);
}

void DesktopMenuModelTest::refusedCapabilitiesAreDisabledNotHidden()
{
    DesktopMenuFacts facts = fullFacts();
    facts.logOut = Capability::disabled();
    facts.paste = Capability::disabled();
    facts.places = Capability::disabled();
    const BuiltDesktopMenu menu = buildDesktopMenu(facts);
    const MenuItem *logOut = findItem(menu.tree.items, QStringLiteral("desktop.log-out"));
    const MenuItem *paste = findItem(menu.tree.items, QStringLiteral("edit.paste"));
    const MenuItem *home = findItem(menu.tree.items, QStringLiteral("go.home"));
    QVERIFY(logOut != nullptr && !logOut->enabled);
    QVERIFY(paste != nullptr && !paste->enabled);
    QVERIFY(home != nullptr && !home->enabled);
    QVERIFY(findItem(menu.tree.items, QStringLiteral("desktop.lock-screen"))->enabled);
}

void DesktopMenuModelTest::desktopIconsEntriesFollowTheSurface()
{
    DesktopMenuFacts facts = fullFacts();
    facts.newFolder = {};
    facts.paste = {};
    facts.selectAll = {};
    facts.cleanUp = {};
    const BuiltDesktopMenu menu = buildDesktopMenu(facts);
    QCOMPARE(texts(topLevel(menu, "desktop.menu.file").children),
             (QStringList{QStringLiteral("New File Manager Window"), QStringLiteral("—"),
                          QStringLiteral("Find…")}));
    QCOMPARE(texts(topLevel(menu, "desktop.menu.edit").children),
             QStringList{QStringLiteral("Show Clipboard History")});
    QCOMPARE(texts(topLevel(menu, "desktop.menu.view").children),
             (QStringList{QStringLiteral("Show Desktop"), QStringLiteral("Gather Overview"),
                          QStringLiteral("—"), QStringLiteral("Edit Panels")}));
    verifySeparatorsOnlyBetweenGroups(menu.tree.items);
}

void DesktopMenuModelTest::hostileWorkspacesStillValidate()
{
    DesktopMenuFacts facts = fullFacts();
    const QString oversizedId(QindaQt::Shell::GlobalMenu::Protocol::kMaxIdUtf8Bytes, QLatin1Char('x'));
    QString brokenName = QStringLiteral("Bad");
    brokenName.append(QChar(0xD800)); // unpaired high surrogate
    brokenName.append(QChar(0));
    brokenName.append(QStringLiteral("Name"));
    facts.workspaceList = {
        {QStringLiteral("a"), QString(4000, QLatin1Char('W')), true},
        {QStringLiteral("a"), QStringLiteral("Duplicate"), false},
        {oversizedId, QStringLiteral("Too long id"), false},
        {QStringLiteral("b"), brokenName, true},
        {QStringLiteral("c"), QStringLiteral("   "), false},
        {QString{}, QStringLiteral("No id"), false},
    };
    for (int i = 0; i < 200; ++i) {
        facts.workspaceList.append({QStringLiteral("extra-%1").arg(i), QStringLiteral("E"), false});
    }
    const BuiltDesktopMenu menu = buildDesktopMenu(facts);
    const auto validation =
        QindaQt::Shell::GlobalMenu::Protocol::validateMenuTree(menu.tree);
    QVERIFY2(validation.accepted, qPrintable(validation.reasonCode + validation.path));
    const MenuItem &window = topLevel(menu, "desktop.menu.window");
    QVERIFY(window.children.size() <= 64);
    const MenuItem *first = findItem(menu.tree.items, QStringLiteral("desktop.workspace.a"));
    QVERIFY(first != nullptr && first->checked);
    QVERIFY(first->text.size() <= 96);
    const MenuItem *broken = findItem(menu.tree.items, QStringLiteral("desktop.workspace.b"));
    QVERIFY(broken != nullptr);
    QVERIFY(!broken->checked); // one checked member per radio group
    QCOMPARE(broken->text, QStringLiteral("BadName"));
    const MenuItem *blank = findItem(menu.tree.items, QStringLiteral("desktop.workspace.c"));
    QVERIFY(blank != nullptr);
    QCOMPARE(blank->text, QStringLiteral("Workspace 3"));
    QVERIFY(!menu.commands.contains(QStringLiteral("desktop.workspace.") + oversizedId));
}

void DesktopMenuModelTest::titleAndIconComeFromTheCatalog()
{
    QCOMPARE(desktopMenuTitle(), Catalog::applicationTitle());
    QCOMPARE(desktopMenuTitle(), QStringLiteral("File Manager"));
    QCOMPARE(desktopMenuIconName(), Catalog::applicationIconName());
    // The catalog mirrors File Manager's installed desktop entry.
    QFile entry(QStringLiteral(QINDAQT_SOURCE_DIR
                               "/src/apps/file_manager/org.qindaqt.FileManager.desktop"));
    QVERIFY(entry.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString text = QString::fromUtf8(entry.readAll());
    QVERIFY(text.contains(QStringLiteral("\nGenericName=%1\n").arg(Catalog::applicationTitle())));
    QVERIFY(text.contains(QStringLiteral("\nIcon=%1\n").arg(Catalog::applicationIconName())));
}

void DesktopMenuModelTest::sessionEndingCommandsAskFirst()
{
    for (const DesktopCommand kind :
         {DesktopCommand::LogOut, DesktopCommand::Restart, DesktopCommand::ShutDown}) {
        QVERIFY(commandNeedsConfirmation(kind));
        QVERIFY(!confirmationTitle(kind).isEmpty());
        QVERIFY(!confirmationText(kind).isEmpty());
    }
    for (const DesktopCommand kind : {DesktopCommand::LockScreen, DesktopCommand::Suspend,
                                      DesktopCommand::NewFolder, DesktopCommand::OpenPlace}) {
        QVERIFY(!commandNeedsConfirmation(kind));
    }
    QCOMPARE(confirmationTitle(DesktopCommand::LogOut), QStringLiteral("Log out?"));
}

QTEST_GUILESS_MAIN(DesktopMenuModelTest)

#include "tst_desktop_menu_model.moc"
