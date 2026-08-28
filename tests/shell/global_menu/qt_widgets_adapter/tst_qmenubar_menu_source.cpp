// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/exporter/menu_source.h>
#include <qindaqt/shell/global_menu/protocol/menu_item_lookup.h>
#include <qindaqt/shell/global_menu/protocol/menu_limits.h>
#include <qindaqt/shell/global_menu/protocol/menu_validation.h>
#include <qindaqt/shell/global_menu/qt_widgets_adapter/qmenubar_menu_source.h>

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QMenuBar>
#include <QTest>

using namespace QindaQt::Shell::GlobalMenu;
using namespace QindaQt::Shell::GlobalMenu::Protocol;
using namespace QindaQt::Shell::GlobalMenu::QtWidgetsAdapter;

namespace {

// Mirrors the integrated Text Editor's ADR-0022 menu shape (persistent
// object names, '&' mnemonics, QKeySequence shortcuts) plus a checkable
// exclusive group and a disabled item, so the adapter is proven against
// every role G0 must support.
QMenuBar *buildFixtureMenuBar(QObject *parent)
{
    auto *menuBar = new QMenuBar();
    menuBar->setParent(parent);

    QMenu *fileMenu = menuBar->addMenu(QStringLiteral("&File"));
    fileMenu->setObjectName(QStringLiteral("fileMenu"));

    auto *newAction = new QAction(QStringLiteral("&New"), menuBar);
    newAction->setObjectName(QStringLiteral("fileNewAction"));
    newAction->setShortcut(QKeySequence::New);
    fileMenu->addAction(newAction);

    fileMenu->addSeparator();

    auto *quitAction = new QAction(QStringLiteral("&Quit"), menuBar);
    quitAction->setObjectName(QStringLiteral("fileQuitAction"));
    quitAction->setShortcut(QKeySequence::Quit);
    quitAction->setEnabled(false);
    fileMenu->addAction(quitAction);

    auto *hiddenAction = new QAction(QStringLiteral("&Hidden"), menuBar);
    hiddenAction->setObjectName(QStringLiteral("fileHiddenAction"));
    hiddenAction->setVisible(false);
    fileMenu->addAction(hiddenAction);

    QMenu *viewMenu = menuBar->addMenu(QStringLiteral("&View"));
    viewMenu->setObjectName(QStringLiteral("viewMenu"));

    auto *wordWrapAction = new QAction(QStringLiteral("&Word Wrap"), menuBar);
    wordWrapAction->setObjectName(QStringLiteral("viewWordWrapAction"));
    wordWrapAction->setCheckable(true);
    wordWrapAction->setChecked(true);
    viewMenu->addAction(wordWrapAction);

    auto *alignmentGroup = new QActionGroup(menuBar);
    alignmentGroup->setObjectName(QStringLiteral("alignmentGroup"));
    alignmentGroup->setExclusive(true);

    auto *alignLeftAction = new QAction(QStringLiteral("Align &Left"), menuBar);
    alignLeftAction->setObjectName(QStringLiteral("viewAlignLeftAction"));
    alignLeftAction->setCheckable(true);
    alignLeftAction->setChecked(true);
    alignLeftAction->setActionGroup(alignmentGroup);
    viewMenu->addAction(alignLeftAction);

    auto *alignRightAction = new QAction(QStringLiteral("Align &Right"), menuBar);
    alignRightAction->setObjectName(QStringLiteral("viewAlignRightAction"));
    alignRightAction->setCheckable(true);
    alignRightAction->setActionGroup(alignmentGroup);
    viewMenu->addAction(alignRightAction);

    return menuBar;
}

} // namespace

class QMenuBarMenuSourceTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void producesAValidatedTree();
    void preservesObjectNamesAsIds();
    void splitsMnemonicFromDisplayText();
    void marksDisabledActionAsDisabled();
    void marksInvisibleActionAsInvisible();
    void mapsExclusiveActionGroupToSameRadioGroup();
    void translatesSeparatorCanonically();
    void snapshotIsEmptyAfterMenuBarDestroyed();
    void overflowDepthIsIncompleteNotTruncated();
    void overflowSiblingsAreIncompleteNotTruncated();
    void overflowTotalItemsAreIncompleteNotTruncated();
    void submenuCycleIsIncomplete();
};

void QMenuBarMenuSourceTests::producesAValidatedTree()
{
    QObject owner;
    QMenuBar *menuBar = buildFixtureMenuBar(&owner);
    QMenuBarMenuSource source(menuBar, QUuid::createUuid());
    const MenuSnapshot snapshot = source.snapshot();
    QVERIFY(snapshot.complete);
    const ValidationResult result = validateMenuTree(snapshot.tree);
    QVERIFY2(result.accepted, qPrintable(result.reasonCode + QStringLiteral(" @ ") + result.path));
}

void QMenuBarMenuSourceTests::preservesObjectNamesAsIds()
{
    QObject owner;
    QMenuBar *menuBar = buildFixtureMenuBar(&owner);
    QMenuBarMenuSource source(menuBar, QUuid::createUuid());
    const MenuTree tree = source.snapshot().tree;
    QVERIFY(findMenuItemById(tree.items, QStringLiteral("fileNewAction")) != nullptr);
    QVERIFY(findMenuItemById(tree.items, QStringLiteral("fileQuitAction")) != nullptr);
}

void QMenuBarMenuSourceTests::splitsMnemonicFromDisplayText()
{
    QObject owner;
    QMenuBar *menuBar = buildFixtureMenuBar(&owner);
    QMenuBarMenuSource source(menuBar, QUuid::createUuid());
    const MenuTree tree = source.snapshot().tree;
    const MenuItem *newAction = findMenuItemById(tree.items, QStringLiteral("fileNewAction"));
    QVERIFY(newAction != nullptr);
    QCOMPARE(newAction->text, QStringLiteral("New"));
    QCOMPARE(newAction->mnemonicIndex, 0);
    QVERIFY(!newAction->shortcutText.isEmpty());
}

void QMenuBarMenuSourceTests::marksDisabledActionAsDisabled()
{
    QObject owner;
    QMenuBar *menuBar = buildFixtureMenuBar(&owner);
    QMenuBarMenuSource source(menuBar, QUuid::createUuid());
    const MenuTree tree = source.snapshot().tree;
    const MenuItem *quitAction = findMenuItemById(tree.items, QStringLiteral("fileQuitAction"));
    QVERIFY(quitAction != nullptr);
    QVERIFY(!quitAction->enabled);
}

void QMenuBarMenuSourceTests::marksInvisibleActionAsInvisible()
{
    QObject owner;
    QMenuBar *menuBar = buildFixtureMenuBar(&owner);
    QMenuBarMenuSource source(menuBar, QUuid::createUuid());
    const MenuTree tree = source.snapshot().tree;
    // The adapter must carry visibility verbatim (not drop or invert it) so
    // presentation can omit hidden entries honestly.
    const MenuItem *hiddenAction = findMenuItemById(tree.items, QStringLiteral("fileHiddenAction"));
    QVERIFY(hiddenAction != nullptr);
    QVERIFY(!hiddenAction->visible);
    const MenuItem *visibleAction = findMenuItemById(tree.items, QStringLiteral("fileNewAction"));
    QVERIFY(visibleAction != nullptr);
    QVERIFY(visibleAction->visible);
}

void QMenuBarMenuSourceTests::mapsExclusiveActionGroupToSameRadioGroup()
{
    QObject owner;
    QMenuBar *menuBar = buildFixtureMenuBar(&owner);
    QMenuBarMenuSource source(menuBar, QUuid::createUuid());
    const MenuTree tree = source.snapshot().tree;
    const MenuItem *left = findMenuItemById(tree.items, QStringLiteral("viewAlignLeftAction"));
    const MenuItem *right = findMenuItemById(tree.items, QStringLiteral("viewAlignRightAction"));
    QVERIFY(left != nullptr && right != nullptr);
    QVERIFY(!left->radioGroup.isEmpty());
    QCOMPARE(left->radioGroup, right->radioGroup);
    QVERIFY(left->checked);
    QVERIFY(!right->checked);

    const MenuItem *wordWrap = findMenuItemById(tree.items, QStringLiteral("viewWordWrapAction"));
    QVERIFY(wordWrap != nullptr);
    QVERIFY(wordWrap->radioGroup.isEmpty());
    QVERIFY(wordWrap->checkable);
    QVERIFY(wordWrap->checked);
}

void QMenuBarMenuSourceTests::translatesSeparatorCanonically()
{
    QObject owner;
    QMenuBar *menuBar = buildFixtureMenuBar(&owner);
    QMenuBarMenuSource source(menuBar, QUuid::createUuid());
    const MenuTree tree = source.snapshot().tree;
    const MenuItem *fileMenu = findMenuItemById(tree.items, QStringLiteral("fileMenu"));
    QVERIFY(fileMenu != nullptr);
    bool sawSeparator = false;
    for (const MenuItem &child : fileMenu->children) {
        if (child.kind == MenuItemKind::Separator) {
            sawSeparator = true;
            QVERIFY(child.text.isEmpty());
        }
    }
    QVERIFY(sawSeparator);
}

void QMenuBarMenuSourceTests::snapshotIsEmptyAfterMenuBarDestroyed()
{
    auto *menuBar = new QMenuBar();
    QMenuBarMenuSource source(menuBar, QUuid::createUuid());
    delete menuBar;
    const MenuSnapshot snapshot = source.snapshot();
    QVERIFY(snapshot.complete);
    QVERIFY(snapshot.tree.items.isEmpty());
}

void QMenuBarMenuSourceTests::overflowDepthIsIncompleteNotTruncated()
{
    QObject owner;
    auto *menuBar = new QMenuBar();
    menuBar->setParent(&owner);

    // Chain kMaxDepth + 2 menus so the deepest item sits beyond the canonical
    // depth bound. The walk must report an incomplete snapshot instead of a
    // truncated-but-valid prefix.
    QMenu *level = menuBar->addMenu(QStringLiteral("&Level0"));
    level->setObjectName(QStringLiteral("level0"));
    for (int depth = 1; depth <= kMaxDepth + 1; ++depth) {
        auto *next = new QMenu(QStringLiteral("Level%1").arg(depth), menuBar);
        next->setObjectName(QStringLiteral("level%1").arg(depth));
        level->addMenu(next);
        level = next;
    }

    QMenuBarMenuSource source(menuBar, QUuid::createUuid());
    const MenuSnapshot snapshot = source.snapshot();
    QVERIFY(!snapshot.complete);
    QVERIFY(snapshot.tree.items.isEmpty());
}

void QMenuBarMenuSourceTests::overflowSiblingsAreIncompleteNotTruncated()
{
    QObject owner;
    auto *menuBar = new QMenuBar();
    menuBar->setParent(&owner);

    QMenu *bigMenu = menuBar->addMenu(QStringLiteral("&Big"));
    bigMenu->setObjectName(QStringLiteral("bigMenu"));
    for (int index = 0; index < Protocol::kMaxChildrenPerItem + 1; ++index) {
        auto *child = new QAction(QStringLiteral("Item %1").arg(index), bigMenu);
        bigMenu->addAction(child);
    }

    QMenuBarMenuSource source(menuBar, QUuid::createUuid());
    const MenuSnapshot snapshot = source.snapshot();
    QVERIFY(!snapshot.complete);
    QVERIFY(snapshot.tree.items.isEmpty());
}

void QMenuBarMenuSourceTests::overflowTotalItemsAreIncompleteNotTruncated()
{
    QObject owner;
    auto *menuBar = new QMenuBar();
    menuBar->setParent(&owner);

    // Nine fully loaded menus stay within the per-sibling bound but exceed
    // the whole-tree budget (9 + 9*kMaxChildrenPerItem > kMaxTotalItems).
    for (int menuIndex = 0; menuIndex < 9; ++menuIndex) {
        QMenu *menu = menuBar->addMenu(QStringLiteral("&Wide%1").arg(menuIndex));
        menu->setObjectName(QStringLiteral("wideMenu%1").arg(menuIndex));
        for (int index = 0; index < Protocol::kMaxChildrenPerItem; ++index) {
            auto *child = new QAction(QStringLiteral("Row %1").arg(index), menu);
            menu->addAction(child);
        }
    }

    QMenuBarMenuSource source(menuBar, QUuid::createUuid());
    const MenuSnapshot snapshot = source.snapshot();
    QVERIFY(!snapshot.complete);
    QVERIFY(snapshot.tree.items.isEmpty());
}

void QMenuBarMenuSourceTests::submenuCycleIsIncomplete()
{
    QObject owner;
    auto *menuBar = new QMenuBar();
    menuBar->setParent(&owner);

    // Public Qt API permits mutual submenu references; the walk must detect
    // the revisit and fail the snapshot instead of looping or truncating.
    QMenu *outer = menuBar->addMenu(QStringLiteral("&Outer"));
    outer->setObjectName(QStringLiteral("outerMenu"));
    auto *inner = new QMenu(QStringLiteral("Inner"), menuBar);
    inner->setObjectName(QStringLiteral("innerMenu"));
    outer->addMenu(inner);
    inner->addMenu(outer);

    QMenuBarMenuSource source(menuBar, QUuid::createUuid());
    const MenuSnapshot snapshot = source.snapshot();
    QVERIFY(!snapshot.complete);
    QVERIFY(snapshot.tree.items.isEmpty());
}

QTEST_MAIN(QMenuBarMenuSourceTests)
#include "tst_qmenubar_menu_source.moc"
