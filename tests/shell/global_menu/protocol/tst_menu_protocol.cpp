// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/global_menu/protocol/menu_item_lookup.h>
#include <qindaqt/shell/global_menu/protocol/menu_limits.h>
#include <qindaqt/shell/global_menu/protocol/menu_validation.h>

#include <QtTest>

using namespace QindaQt::Shell::GlobalMenu::Protocol;

namespace {

MenuItem action(const QString &id, const QString &text)
{
    MenuItem item;
    item.id = id;
    item.kind = MenuItemKind::Action;
    item.text = text;
    return item;
}

MenuItem separator(const QString &id)
{
    MenuItem item;
    item.id = id;
    item.kind = MenuItemKind::Separator;
    return item;
}

MenuTree simpleValidTree()
{
    MenuItem file;
    file.id = QStringLiteral("fileMenu");
    file.kind = MenuItemKind::Submenu;
    file.text = QStringLiteral("File");
    file.mnemonicIndex = 0;
    file.children = {action(QStringLiteral("fileNewAction"), QStringLiteral("New")),
                      separator(QStringLiteral("fileSep1")),
                      action(QStringLiteral("fileQuitAction"), QStringLiteral("Quit"))};

    MenuTree tree;
    tree.ownerWindowId = QUuid::createUuid();
    tree.epoch = QUuid::createUuid();
    tree.revision = 1;
    tree.items = {file};
    return tree;
}

} // namespace

class MenuProtocolTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void acceptsWellFormedTree();
    void rejectsDuplicateIds();
    void rejectsEmptyId();
    void rejectsOversizedText();
    void rejectsInvalidMnemonicIndex();
    void rejectsSeparatorWithContent();
    void rejectsCheckedWithoutCheckable();
    void rejectsRadioGroupWithoutCheckable();
    void rejectsMultipleCheckedInSameRadioGroup();
    void acceptsMultipleCheckedInDifferentRadioGroups();
    void rejectsActionWithChildren();
    void rejectsTooDeepTree();
    void rejectsTooManyChildren();
    void rejectsTooManyTotalItems();
    void acceptsEmptyTree();
    void rejectsUnknownItemKind();
    void rejectsIsolatedHighSurrogate();
    void rejectsIsolatedLowSurrogate();
    void acceptsPairedSurrogateText();

    void lookupFindsNestedItemById();
    void lookupReturnsNullForUnknownId();
};

void MenuProtocolTests::acceptsWellFormedTree()
{
    const ValidationResult result = validateMenuTree(simpleValidTree());
    QVERIFY(result.accepted);
    QVERIFY(result.reasonCode.isEmpty());
}

void MenuProtocolTests::rejectsDuplicateIds()
{
    MenuTree tree = simpleValidTree();
    tree.items.append(action(QStringLiteral("fileNewAction"), QStringLiteral("Duplicate")));
    const ValidationResult result = validateMenuTree(tree);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("duplicate-id"));
}

void MenuProtocolTests::rejectsEmptyId()
{
    MenuTree tree;
    tree.items = {action(QString(), QStringLiteral("No id"))};
    const ValidationResult result = validateMenuTree(tree);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("invalid-id"));
}

void MenuProtocolTests::rejectsOversizedText()
{
    MenuTree tree;
    tree.items = {action(QStringLiteral("a"), QString(kMaxTextUtf8Bytes + 1, QLatin1Char('x')))};
    const ValidationResult result = validateMenuTree(tree);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("invalid-text"));
}

void MenuProtocolTests::rejectsInvalidMnemonicIndex()
{
    MenuTree tree;
    MenuItem item = action(QStringLiteral("a"), QStringLiteral("New"));
    item.mnemonicIndex = 99;
    tree.items = {item};
    const ValidationResult result = validateMenuTree(tree);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("invalid-mnemonic-index"));
}

void MenuProtocolTests::rejectsSeparatorWithContent()
{
    MenuTree tree;
    MenuItem item = separator(QStringLiteral("sep"));
    item.text = QStringLiteral("not empty");
    tree.items = {item};
    const ValidationResult result = validateMenuTree(tree);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("separator-has-content"));
}

void MenuProtocolTests::rejectsCheckedWithoutCheckable()
{
    MenuTree tree;
    MenuItem item = action(QStringLiteral("a"), QStringLiteral("Word wrap"));
    item.checked = true;
    tree.items = {item};
    const ValidationResult result = validateMenuTree(tree);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("checked-without-checkable"));
}

void MenuProtocolTests::rejectsRadioGroupWithoutCheckable()
{
    MenuTree tree;
    MenuItem item = action(QStringLiteral("a"), QStringLiteral("Left"));
    item.radioGroup = QStringLiteral("alignment");
    tree.items = {item};
    const ValidationResult result = validateMenuTree(tree);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("radio-group-without-checkable"));
}

void MenuProtocolTests::rejectsMultipleCheckedInSameRadioGroup()
{
    MenuTree tree;
    MenuItem left = action(QStringLiteral("left"), QStringLiteral("Left"));
    left.checkable = true;
    left.checked = true;
    left.radioGroup = QStringLiteral("alignment");
    MenuItem right = action(QStringLiteral("right"), QStringLiteral("Right"));
    right.checkable = true;
    right.checked = true;
    right.radioGroup = QStringLiteral("alignment");
    tree.items = {left, right};
    const ValidationResult result = validateMenuTree(tree);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("multiple-checked-in-radio-group"));
}

void MenuProtocolTests::acceptsMultipleCheckedInDifferentRadioGroups()
{
    MenuTree tree;
    MenuItem left = action(QStringLiteral("left"), QStringLiteral("Left"));
    left.checkable = true;
    left.checked = true;
    left.radioGroup = QStringLiteral("alignment");
    MenuItem twelvePt = action(QStringLiteral("twelvePt"), QStringLiteral("12pt"));
    twelvePt.checkable = true;
    twelvePt.checked = true;
    twelvePt.radioGroup = QStringLiteral("size");
    tree.items = {left, twelvePt};
    const ValidationResult result = validateMenuTree(tree);
    QVERIFY(result.accepted);
}

void MenuProtocolTests::rejectsActionWithChildren()
{
    MenuTree tree;
    MenuItem item = action(QStringLiteral("a"), QStringLiteral("New"));
    item.children = {action(QStringLiteral("b"), QStringLiteral("Nested"))};
    tree.items = {item};
    const ValidationResult result = validateMenuTree(tree);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("action-has-children"));
}

void MenuProtocolTests::rejectsTooDeepTree()
{
    MenuItem leaf = action(QStringLiteral("leaf"), QStringLiteral("Leaf"));
    for (int depth = 0; depth < kMaxDepth + 1; ++depth) {
        MenuItem submenu;
        submenu.id = QStringLiteral("submenu-%1").arg(depth);
        submenu.kind = MenuItemKind::Submenu;
        submenu.text = QStringLiteral("Level %1").arg(depth);
        submenu.children = {leaf};
        leaf = submenu;
    }
    MenuTree tree;
    tree.items = {leaf};
    const ValidationResult result = validateMenuTree(tree);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("too-deep"));
}

void MenuProtocolTests::rejectsTooManyChildren()
{
    MenuItem submenu;
    submenu.id = QStringLiteral("submenu");
    submenu.kind = MenuItemKind::Submenu;
    submenu.text = QStringLiteral("Big");
    for (int index = 0; index < kMaxChildrenPerItem + 1; ++index) {
        submenu.children.append(action(QStringLiteral("item-%1").arg(index), QStringLiteral("Item")));
    }
    MenuTree tree;
    tree.items = {submenu};
    const ValidationResult result = validateMenuTree(tree);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("too-many-children"));
}

void MenuProtocolTests::rejectsTooManyTotalItems()
{
    MenuTree tree;
    for (int index = 0; index < kMaxTotalItems + 1; ++index) {
        tree.items.append(action(QStringLiteral("item-%1").arg(index), QStringLiteral("Item")));
    }
    const ValidationResult result = validateMenuTree(tree);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("too-many-items"));
}

void MenuProtocolTests::acceptsEmptyTree()
{
    MenuTree tree;
    const ValidationResult result = validateMenuTree(tree);
    QVERIFY(result.accepted);
}

void MenuProtocolTests::rejectsUnknownItemKind()
{
    // A hostile wire value must not slip past the switch on known kinds; in
    // particular its children must not be treated as traversable content.
    MenuTree tree;
    MenuItem hostile = action(QStringLiteral("hostile"), QStringLiteral("Hostile"));
    hostile.kind = static_cast<MenuItemKind>(99);
    hostile.children = {action(QStringLiteral("smuggled"), QStringLiteral("Smuggled"))};
    tree.items = {hostile};
    const ValidationResult result = validateMenuTree(tree);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("unknown-kind"));
}

void MenuProtocolTests::rejectsIsolatedHighSurrogate()
{
    MenuTree tree;
    QString text = QStringLiteral("Broken ");
    text.append(QChar(0xD83D)); // high surrogate with no low surrogate after it
    tree.items = {action(QStringLiteral("a"), text)};
    const ValidationResult result = validateMenuTree(tree);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("invalid-text"));
}

void MenuProtocolTests::rejectsIsolatedLowSurrogate()
{
    MenuTree tree;
    QString text = QStringLiteral("Broken ");
    text.append(QChar(0xDE00)); // low surrogate with no high surrogate before it
    tree.items = {action(QStringLiteral("a"), text)};
    const ValidationResult result = validateMenuTree(tree);
    QVERIFY(!result.accepted);
    QCOMPARE(result.reasonCode, QStringLiteral("invalid-text"));
}

void MenuProtocolTests::acceptsPairedSurrogateText()
{
    MenuTree tree;
    QString text = QStringLiteral("Save ");
    text.append(QChar(0xD83D));
    text.append(QChar(0xDE00)); // U+1F600 as a correctly paired surrogate pair
    tree.items = {action(QStringLiteral("a"), text)};
    const ValidationResult result = validateMenuTree(tree);
    QVERIFY(result.accepted);
}

void MenuProtocolTests::lookupFindsNestedItemById()
{
    const MenuTree tree = simpleValidTree();
    const MenuItem *found = findMenuItemById(tree.items, QStringLiteral("fileQuitAction"));
    QVERIFY(found != nullptr);
    QCOMPARE(found->text, QStringLiteral("Quit"));
}

void MenuProtocolTests::lookupReturnsNullForUnknownId()
{
    const MenuTree tree = simpleValidTree();
    QVERIFY(findMenuItemById(tree.items, QStringLiteral("does-not-exist")) == nullptr);
}

QTEST_APPLESS_MAIN(MenuProtocolTests)
#include "tst_menu_protocol.moc"
