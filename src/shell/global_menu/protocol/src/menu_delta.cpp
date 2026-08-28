// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/protocol/menu_delta.h>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QString>

namespace QindaQt::Shell::GlobalMenu::Protocol
{

namespace
{

struct FlatNode final {
    QString id;
    QString parentId;
    int siblingIndex = 0;
    MenuItemKind kind = MenuItemKind::Action;
    QString text;
    int mnemonicIndex = -1;
    QString shortcutText;
    bool enabled = true;
    bool visible = true;
    bool checkable = false;
    bool checked = false;
    QString radioGroup;
};

bool shallowEquals(const FlatNode &a, const FlatNode &b)
{
    return a.parentId == b.parentId && a.siblingIndex == b.siblingIndex && a.kind == b.kind
        && a.text == b.text && a.mnemonicIndex == b.mnemonicIndex
        && a.shortcutText == b.shortcutText && a.enabled == b.enabled && a.visible == b.visible
        && a.checkable == b.checkable && a.checked == b.checked && a.radioGroup == b.radioGroup;
}

void flattenInto(const QList<MenuItem> &items, const QString &parentId, QList<FlatNode> &out)
{
    for (qsizetype index = 0; index < items.size(); ++index) {
        const MenuItem &item = items.at(index);
        out.append(FlatNode{.id = item.id,
                             .parentId = parentId,
                             .siblingIndex = static_cast<int>(index),
                             .kind = item.kind,
                             .text = item.text,
                             .mnemonicIndex = item.mnemonicIndex,
                             .shortcutText = item.shortcutText,
                             .enabled = item.enabled,
                             .visible = item.visible,
                             .checkable = item.checkable,
                             .checked = item.checked,
                             .radioGroup = item.radioGroup});
        flattenInto(item.children, item.id, out);
    }
}

QList<FlatNode> flatten(const MenuTree &tree)
{
    QList<FlatNode> flat;
    flattenInto(tree.items, QString(), flat);
    return flat;
}

} // namespace

MenuTreeDelta computeMenuTreeDelta(const MenuTree &previous, const MenuTree &next)
{
    const QList<FlatNode> previousFlat = flatten(previous);
    const QList<FlatNode> nextFlat = flatten(next);

    QHash<QString, const FlatNode *> previousById;
    previousById.reserve(previousFlat.size());
    for (const FlatNode &node : previousFlat) {
        previousById.insert(node.id, &node);
    }
    QHash<QString, const FlatNode *> nextById;
    nextById.reserve(nextFlat.size());
    for (const FlatNode &node : nextFlat) {
        nextById.insert(node.id, &node);
    }

    MenuTreeDelta delta;
    for (const FlatNode &node : previousFlat) {
        if (!nextById.contains(node.id)) {
            delta.operations.append(
                MenuItemDelta{.op = MenuDeltaOp::Removed, .id = node.id, .parentId = node.parentId});
        }
    }
    for (const FlatNode &node : nextFlat) {
        if (!previousById.contains(node.id)) {
            delta.operations.append(
                MenuItemDelta{.op = MenuDeltaOp::Inserted, .id = node.id, .parentId = node.parentId});
        }
    }
    for (const FlatNode &node : nextFlat) {
        const auto it = previousById.constFind(node.id);
        if (it == previousById.constEnd()) {
            continue;
        }
        if (!shallowEquals(**it, node)) {
            delta.operations.append(
                MenuItemDelta{.op = MenuDeltaOp::Updated, .id = node.id, .parentId = node.parentId});
        }
    }
    return delta;
}

} // namespace QindaQt::Shell::GlobalMenu::Protocol
