// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/protocol/menu_limits.h>
#include <qindaqt/shell/global_menu/protocol/menu_validation.h>

#include <QtCore/QSet>
#include <QtCore/QString>

namespace QindaQt::Shell::GlobalMenu::Protocol
{

namespace
{

bool isWellFormedBoundedText(const QString &value, qsizetype maxUtf8Bytes)
{
    if (value.toUtf8().size() > maxUtf8Bytes) {
        return false;
    }
    for (qsizetype i = 0; i < value.size(); ++i) {
        const QChar ch = value.at(i);
        if (ch == QChar(u'\0')) {
            return false;
        }
        // QString storage does not guarantee paired surrogate encoding; an
        // isolated surrogate is not a representable Unicode scalar value and
        // must not enter the canonical model.
        if (ch.isHighSurrogate()) {
            if (i + 1 >= value.size() || !value.at(i + 1).isLowSurrogate()) {
                return false;
            }
            ++i;
        } else if (ch.isLowSurrogate()) {
            return false;
        }
    }
    return true;
}

ValidationResult reject(QString reasonCode, QString path)
{
    return ValidationResult{.accepted = false,
                             .reasonCode = std::move(reasonCode),
                             .path = std::move(path)};
}

struct WalkState {
    int totalItems = 0;
    QSet<QString> seenIds;
};

ValidationResult validateNode(const MenuItem &item, int depth, const QString &path,
                               WalkState &state)
{
    if (depth > kMaxDepth) {
        return reject(QStringLiteral("too-deep"), path);
    }
    if (++state.totalItems > kMaxTotalItems) {
        return reject(QStringLiteral("too-many-items"), path);
    }

    if (item.id.isEmpty() || !isWellFormedBoundedText(item.id, kMaxIdUtf8Bytes)) {
        return reject(QStringLiteral("invalid-id"), path);
    }
    if (state.seenIds.contains(item.id)) {
        return reject(QStringLiteral("duplicate-id"), path);
    }
    state.seenIds.insert(item.id);

    // AGENT-GUARD: an out-of-range MenuItemKind (hostile cast into the enum)
    // must reject the node before any content rule runs; its children would
    // otherwise bypass traversal entirely.
    switch (item.kind) {
    case MenuItemKind::Action:
    case MenuItemKind::Separator:
    case MenuItemKind::Submenu:
        break;
    default:
        return reject(QStringLiteral("unknown-kind"), path);
    }

    if (item.kind == MenuItemKind::Separator) {
        const bool separatorIsCanonical = item.text.isEmpty() && item.mnemonicIndex == -1
            && item.shortcutText.isEmpty() && !item.checkable && !item.checked
            && item.radioGroup.isEmpty() && item.children.isEmpty();
        if (!separatorIsCanonical) {
            return reject(QStringLiteral("separator-has-content"), path);
        }
        return ValidationResult{.accepted = true};
    }

    if (item.text.isEmpty() || !isWellFormedBoundedText(item.text, kMaxTextUtf8Bytes)) {
        return reject(QStringLiteral("invalid-text"), path);
    }
    if (item.mnemonicIndex != -1
        && (item.mnemonicIndex < 0 || item.mnemonicIndex >= item.text.size())) {
        return reject(QStringLiteral("invalid-mnemonic-index"), path);
    }
    if (!isWellFormedBoundedText(item.shortcutText, kMaxShortcutUtf8Bytes)) {
        return reject(QStringLiteral("invalid-shortcut"), path);
    }
    if (!isWellFormedBoundedText(item.radioGroup, kMaxRadioGroupUtf8Bytes)) {
        return reject(QStringLiteral("invalid-radio-group"), path);
    }
    if (item.checked && !item.checkable) {
        return reject(QStringLiteral("checked-without-checkable"), path);
    }
    if (!item.radioGroup.isEmpty() && !item.checkable) {
        return reject(QStringLiteral("radio-group-without-checkable"), path);
    }

    if (item.kind == MenuItemKind::Action && !item.children.isEmpty()) {
        return reject(QStringLiteral("action-has-children"), path);
    }
    if (item.kind == MenuItemKind::Submenu && item.children.size() > kMaxChildrenPerItem) {
        return reject(QStringLiteral("too-many-children"), path);
    }

    if (item.kind == MenuItemKind::Submenu) {
        QSet<QString> checkedRadioGroups;
        for (qsizetype index = 0; index < item.children.size(); ++index) {
            const MenuItem &child = item.children.at(index);
            const QString childPath = path + QStringLiteral(".children[%1]").arg(index);
            const ValidationResult childResult = validateNode(child, depth + 1, childPath, state);
            if (!childResult.accepted) {
                return childResult;
            }
            if (child.checked && !child.radioGroup.isEmpty()) {
                if (checkedRadioGroups.contains(child.radioGroup)) {
                    return reject(QStringLiteral("multiple-checked-in-radio-group"), childPath);
                }
                checkedRadioGroups.insert(child.radioGroup);
            }
        }
    }

    return ValidationResult{.accepted = true};
}

} // namespace

ValidationResult validateMenuTree(const MenuTree &tree)
{
    WalkState state;
    QSet<QString> checkedRadioGroups;
    for (qsizetype index = 0; index < tree.items.size(); ++index) {
        const MenuItem &item = tree.items.at(index);
        const QString path = QStringLiteral("items[%1]").arg(index);
        const ValidationResult result = validateNode(item, /*depth=*/1, path, state);
        if (!result.accepted) {
            return result;
        }
        if (item.checked && !item.radioGroup.isEmpty()) {
            if (checkedRadioGroups.contains(item.radioGroup)) {
                return reject(QStringLiteral("multiple-checked-in-radio-group"), path);
            }
            checkedRadioGroups.insert(item.radioGroup);
        }
    }
    return ValidationResult{.accepted = true};
}

} // namespace QindaQt::Shell::GlobalMenu::Protocol
