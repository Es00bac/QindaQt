// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/global_menu/protocol/menu_item_lookup.h>
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
    void mapsExclusiveActionGroupToSameRadioGroup();
    void translatesSeparatorCanonically();
    void snapshotIsEmptyAfterMenuBarDestroyed();
};

void QMenuBarMenuSourceTests::producesAValidatedTree()
{
    QObject owner;
    QMenuBar *menuBar = buildFixtureMenuBar(&owner);
    QMenuBarMenuSource source(menuBar, QUuid::createUuid());
    const MenuTree tree = source.snapshot();
    const ValidationResult result = validateMenuTree(tree);
    QVERIFY2(result.accepted, qPrintable(result.reasonCode + QStringLiteral(" @ ") + result.path));
}

void QMenuBarMenuSourceTests::preservesObjectNamesAsIds()
{
    QObject owner;
    QMenuBar *menuBar = buildFixtureMenuBar(&owner);
    QMenuBarMenuSource source(menuBar, QUuid::createUuid());
    const MenuTree tree = source.snapshot();
    QVERIFY(findMenuItemById(tree.items, QStringLiteral("fileNewAction")) != nullptr);
    QVERIFY(findMenuItemById(tree.items, QStringLiteral("fileQuitAction")) != nullptr);
}

void QMenuBarMenuSourceTests::splitsMnemonicFromDisplayText()
{
    QObject owner;
    QMenuBar *menuBar = buildFixtureMenuBar(&owner);
    QMenuBarMenuSource source(menuBar, QUuid::createUuid());
    const MenuTree tree = source.snapshot();
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
    const MenuTree tree = source.snapshot();
    const MenuItem *quitAction = findMenuItemById(tree.items, QStringLiteral("fileQuitAction"));
    QVERIFY(quitAction != nullptr);
    QVERIFY(!quitAction->enabled);
}

void QMenuBarMenuSourceTests::mapsExclusiveActionGroupToSameRadioGroup()
{
    QObject owner;
    QMenuBar *menuBar = buildFixtureMenuBar(&owner);
    QMenuBarMenuSource source(menuBar, QUuid::createUuid());
    const MenuTree tree = source.snapshot();
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
    const MenuTree tree = source.snapshot();
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
    const MenuTree tree = source.snapshot();
    QVERIFY(tree.items.isEmpty());
}

QTEST_MAIN(QMenuBarMenuSourceTests)
#include "tst_qmenubar_menu_source.moc"
