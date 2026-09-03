// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_decoder.h>

#include <qindaqt/shell/global_menu/protocol/menu_limits.h>
#include <qindaqt/shell/global_menu/protocol/menu_validation.h>

#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusVariant>

#include <optional>

namespace QindaQt::Shell::GlobalMenu::DbusMenu
{

namespace
{

QVariant unwrap(QVariant value)
{
    if (value.metaType() == QMetaType::fromType<QDBusVariant>()) {
        return value.value<QDBusVariant>().variant();
    }
    return value;
}

template<typename T>
std::optional<T> typedProperty(const QVariantMap &properties, const QString &name, bool *valid)
{
    const auto entry = properties.constFind(name);
    if (entry == properties.cend()) {
        return std::nullopt;
    }
    const QVariant value = unwrap(*entry);
    if (value.metaType() != QMetaType::fromType<T>()) {
        *valid = false;
        return std::nullopt;
    }
    return value.value<T>();
}

bool boundedText(const QString &value, qsizetype maximum)
{
    if (value.toUtf8().size() > maximum) {
        return false;
    }
    for (qsizetype index = 0; index < value.size(); ++index) {
        const QChar character = value.at(index);
        if (character == QChar(u'\0')) {
            return false;
        }
        if (character.isHighSurrogate()) {
            if (index + 1 >= value.size() || !value.at(index + 1).isLowSurrogate()) {
                return false;
            }
            ++index;
        } else if (character.isLowSurrogate()) {
            return false;
        }
    }
    return true;
}

std::pair<QString, int> decodeLabel(const QString &label)
{
    QString text;
    text.reserve(label.size());
    int mnemonicIndex = -1;
    for (qsizetype index = 0; index < label.size(); ++index) {
        if (label.at(index) != u'_') {
            text.append(label.at(index));
            continue;
        }
        if (index + 1 < label.size() && label.at(index + 1) == u'_') {
            text.append(u'_');
            ++index;
        } else if (index + 1 < label.size() && mnemonicIndex < 0) {
            mnemonicIndex = static_cast<int>(text.size());
        }
    }
    return {text, mnemonicIndex};
}

QString normalizedShortcut(const ShortcutList &sequences, bool *valid)
{
    if (sequences.size() > kMaxShortcutSequences) {
        *valid = false;
        return {};
    }
    if (sequences.isEmpty()) {
        return {};
    }
    const QStringList &tokens = sequences.first();
    if (tokens.isEmpty() || tokens.size() > kMaxShortcutTokens) {
        *valid = false;
        return {};
    }
    QStringList normalized;
    normalized.reserve(tokens.size());
    for (QString token : tokens) {
        token = token.trimmed();
        if (!boundedText(token, Protocol::kMaxShortcutUtf8Bytes) || token.isEmpty()) {
            *valid = false;
            return {};
        }
        if (token.compare(QStringLiteral("Control"), Qt::CaseInsensitive) == 0) {
            token = QStringLiteral("Ctrl");
        } else if (token.compare(QStringLiteral("Super"), Qt::CaseInsensitive) == 0) {
            token = QStringLiteral("Meta");
        } else if (token.compare(QStringLiteral("Alt"), Qt::CaseInsensitive) == 0) {
            token = QStringLiteral("Alt");
        } else if (token.compare(QStringLiteral("Shift"), Qt::CaseInsensitive) == 0) {
            token = QStringLiteral("Shift");
        }
        if (!normalized.contains(token, Qt::CaseInsensitive)) {
            normalized.append(token);
        }
    }
    const QString result = normalized.join(u'+');
    if (!boundedText(result, Protocol::kMaxShortcutUtf8Bytes)) {
        *valid = false;
        return {};
    }
    return result;
}

std::optional<ShortcutList> shortcutProperty(const QVariantMap &properties, bool *valid)
{
    const auto entry = properties.constFind(QStringLiteral("shortcut"));
    if (entry == properties.cend()) {
        return std::nullopt;
    }
    const QVariant value = unwrap(*entry);
    if (value.canConvert<ShortcutList>()) {
        return value.value<ShortcutList>();
    }
    if (value.metaType() == QMetaType::fromType<QDBusArgument>()) {
        return qdbus_cast<ShortcutList>(value.value<QDBusArgument>());
    }
    *valid = false;
    return std::nullopt;
}

std::optional<LayoutItem> childLayout(const QVariant &wireChild)
{
    const QVariant value = unwrap(wireChild);
    if (value.canConvert<LayoutItem>()) {
        return value.value<LayoutItem>();
    }
    if (value.metaType() == QMetaType::fromType<QDBusArgument>()) {
        return qdbus_cast<LayoutItem>(value.value<QDBusArgument>());
    }
    return std::nullopt;
}

struct WalkState {
    int totalItems = 0;
};

std::optional<Protocol::MenuItem> decodeItem(const LayoutItem &wire, int depth,
                                             WalkState &state, QString *reason)
{
    if (wire.id <= 0) {
        *reason = QStringLiteral("invalid-item-id");
        return std::nullopt;
    }
    if (depth > Protocol::kMaxDepth) {
        *reason = QStringLiteral("too-deep");
        return std::nullopt;
    }
    if (++state.totalItems > Protocol::kMaxTotalItems
        || wire.children.size() > Protocol::kMaxChildrenPerItem) {
        *reason = QStringLiteral("too-many-items");
        return std::nullopt;
    }
    if (wire.properties.size() > kMaxPropertiesPerItem) {
        *reason = QStringLiteral("too-many-item-properties");
        return std::nullopt;
    }

    bool valid = true;
    const QString type = typedProperty<QString>(wire.properties, QStringLiteral("type"), &valid)
                             .value_or(QString{});
    const QString childrenDisplay =
        typedProperty<QString>(wire.properties, QStringLiteral("children-display"), &valid)
            .value_or(QString{});
    const bool separator = type == QStringLiteral("separator");
    if (!valid || (!type.isEmpty() && !separator)
        || (!childrenDisplay.isEmpty() && childrenDisplay != QStringLiteral("submenu"))) {
        *reason = QStringLiteral("invalid-item-properties");
        return std::nullopt;
    }

    Protocol::MenuItem item;
    item.id = QString::number(wire.id);
    if (separator) {
        item.kind = Protocol::MenuItemKind::Separator;
        item.enabled = true;
        item.visible = true;
        if (!wire.children.isEmpty()) {
            *reason = QStringLiteral("separator-has-children");
            return std::nullopt;
        }
        return item;
    }

    const std::optional<QString> label =
        typedProperty<QString>(wire.properties, QStringLiteral("label"), &valid);
    const std::optional<QString> iconName =
        typedProperty<QString>(wire.properties, QStringLiteral("icon-name"), &valid);
    const std::optional<QByteArray> iconData =
        typedProperty<QByteArray>(wire.properties, QStringLiteral("icon-data"), &valid);
    const std::optional<bool> enabled =
        typedProperty<bool>(wire.properties, QStringLiteral("enabled"), &valid);
    const std::optional<bool> visible =
        typedProperty<bool>(wire.properties, QStringLiteral("visible"), &valid);
    const std::optional<QString> toggleType =
        typedProperty<QString>(wire.properties, QStringLiteral("toggle-type"), &valid);
    const std::optional<qint32> toggleState =
        typedProperty<qint32>(wire.properties, QStringLiteral("toggle-state"), &valid);
    const std::optional<ShortcutList> shortcuts = shortcutProperty(wire.properties, &valid);
    if (!valid || !label || !boundedText(*label, Protocol::kMaxTextUtf8Bytes)
        || (iconName && !boundedText(*iconName, kMaxIconNameUtf8Bytes))
        || (iconData && iconData->size() > kMaxIconDataBytes)) {
        *reason = QStringLiteral("invalid-item-properties");
        return std::nullopt;
    }
    const auto [text, mnemonic] = decodeLabel(*label);
    if (text.isEmpty() || !boundedText(text, Protocol::kMaxTextUtf8Bytes)) {
        *reason = QStringLiteral("invalid-label");
        return std::nullopt;
    }
    item.text = text;
    item.mnemonicIndex = mnemonic;
    item.enabled = enabled.value_or(true);
    item.visible = visible.value_or(true);
    item.kind = (!wire.children.isEmpty() || childrenDisplay == QStringLiteral("submenu"))
        ? Protocol::MenuItemKind::Submenu
        : Protocol::MenuItemKind::Action;

    if (toggleType) {
        if (*toggleType != QStringLiteral("checkmark") && *toggleType != QStringLiteral("radio")) {
            *reason = QStringLiteral("invalid-toggle-type");
            return std::nullopt;
        }
        item.checkable = true;
        if (*toggleType == QStringLiteral("radio")) {
            item.radioGroup = QStringLiteral("dbusmenu-parent");
        }
    }
    if (toggleState) {
        if (!item.checkable || *toggleState < -1 || *toggleState > 1) {
            *reason = QStringLiteral("invalid-toggle-state");
            return std::nullopt;
        }
        item.checked = *toggleState == 1;
    }
    if (shortcuts) {
        item.shortcutText = normalizedShortcut(*shortcuts, &valid);
        if (!valid) {
            *reason = QStringLiteral("invalid-shortcut");
            return std::nullopt;
        }
    }

    for (const QVariant &child : wire.children) {
        const std::optional<LayoutItem> childWire = childLayout(child);
        if (!childWire) {
            *reason = QStringLiteral("invalid-child");
            return std::nullopt;
        }
        std::optional<Protocol::MenuItem> decoded = decodeItem(*childWire, depth + 1, state, reason);
        if (!decoded) {
            return std::nullopt;
        }
        item.children.append(std::move(*decoded));
    }
    return item;
}

} // namespace

DecodeResult decodeLayout(const QUuid &ownerWindowId, quint32 remoteRevision,
                          const LayoutItem &root)
{
    if (ownerWindowId.isNull() || root.id != 0
        || root.properties.size() > kMaxPropertiesPerItem
        || root.children.size() > Protocol::kMaxChildrenPerItem) {
        return {.accepted = false,
                .reasonCode = QStringLiteral("invalid-root"),
                .snapshot = {}};
    }
    Protocol::MenuTree tree;
    tree.ownerWindowId = ownerWindowId;
    tree.revision = remoteRevision;
    WalkState state;
    QString reason;
    for (const QVariant &child : root.children) {
        const std::optional<LayoutItem> childWire = childLayout(child);
        if (!childWire) {
            return {.accepted = false,
                    .reasonCode = QStringLiteral("invalid-child"),
                    .snapshot = {}};
        }
        std::optional<Protocol::MenuItem> item = decodeItem(*childWire, 1, state, &reason);
        if (!item) {
            return {.accepted = false, .reasonCode = reason, .snapshot = {}};
        }
        tree.items.append(std::move(*item));
    }
    const Protocol::ValidationResult validation = Protocol::validateMenuTree(tree);
    if (!validation.accepted) {
        return {.accepted = false, .reasonCode = validation.reasonCode, .snapshot = {}};
    }
    return {.accepted = true,
            .reasonCode = {},
            .snapshot = Exporter::MenuSnapshot{.tree = std::move(tree),
                                               .complete = true,
                                               .defectCode = {}}};
}

bool validatePropertyUpdates(const PropertyEntryList &updated,
                             const RemovedPropertyEntryList &removed, QString *reasonCode)
{
    const auto reject = [reasonCode](QString reason) {
        if (reasonCode != nullptr) {
            *reasonCode = std::move(reason);
        }
        return false;
    };
    if (updated.size() > kMaxPropertyEntries || removed.size() > kMaxPropertyEntries) {
        return reject(QStringLiteral("too-many-property-updates"));
    }
    qsizetype removedNames = 0;
    for (const PropertyEntry &entry : updated) {
        if (entry.id <= 0 || entry.properties.size() > kMaxPropertiesPerItem) {
            return reject(QStringLiteral("invalid-property-update"));
        }
    }
    for (const RemovedPropertyEntry &entry : removed) {
        removedNames += entry.names.size();
        if (entry.id <= 0 || removedNames > kMaxRemovedPropertyNames) {
            return reject(QStringLiteral("invalid-property-removal"));
        }
        for (const QString &name : entry.names) {
            if (!boundedText(name, 64)) {
                return reject(QStringLiteral("invalid-property-name"));
            }
        }
    }
    if (reasonCode != nullptr) {
        reasonCode->clear();
    }
    return true;
}

} // namespace QindaQt::Shell::GlobalMenu::DbusMenu
