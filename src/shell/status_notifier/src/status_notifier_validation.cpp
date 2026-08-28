// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/status_notifier/status_notifier_validation.h>

namespace QindaQt::StatusNotifier
{
namespace
{

bool containsForbiddenControl(const QString &value)
{
    // NUL and C0 control characters are never meaningful presentation text.
    // DEL is included because it is equally invisible to users.
    for (const QChar character : value) {
        const char16_t code = character.unicode();
        if (code <= 0x001F || code == 0x007F) {
            return true;
        }
    }
    return false;
}

bool isDigit(QChar character)
{
    return character >= u'0' && character <= u'9';
}

ValidationOutcome validateText(const QString &value, qsizetype maxUtf8Bytes, ValidationError error)
{
    if (!isBoundedSafeText(value, maxUtf8Bytes)) {
        return ValidationOutcome::failure(error, QStringLiteral("unbounded-or-control-text"));
    }
    return ValidationOutcome::success();
}

ValidationOutcome validatePixmapList(const QList<Pixmap> &pixmaps, qsizetype maxCount, ValidationError error)
{
    if (pixmaps.size() > maxCount) {
        return ValidationOutcome::failure(error, QStringLiteral("pixmap-count-exceeded"));
    }
    for (const Pixmap &pixmap : pixmaps) {
        const ValidationOutcome outcome = validatePixmap(pixmap);
        if (!outcome.accepted) {
            return outcome;
        }
    }
    return ValidationOutcome::success();
}

} // namespace

ValidationOutcome ValidationOutcome::success()
{
    return ValidationOutcome{true, ValidationError::None, {}};
}

ValidationOutcome ValidationOutcome::failure(ValidationError error, QString reasonCode)
{
    return ValidationOutcome{false, error, std::move(reasonCode)};
}

bool isValidUniqueBusName(const QString &value)
{
    const qsizetype length = value.size();
    if (length < 3 || length > kMaxUniqueNameUtf8Bytes) {
        return false;
    }
    if (!value.startsWith(u':')) {
        return false;
    }
    const qsizetype separator = value.indexOf(u'.');
    if (separator <= 1 || separator == length - 1) {
        return false;
    }
    for (qsizetype index = 1; index < length; ++index) {
        if (index == separator) {
            continue;
        }
        if (!isDigit(value.at(index))) {
            return false;
        }
    }
    return true;
}

bool isValidObjectPath(const QString &value)
{
    const qsizetype length = value.size();
    if (length < 2 || length > kMaxObjectPathUtf8Bytes) {
        return false;
    }
    if (!value.startsWith(u'/')) {
        return false;
    }
    if (value.endsWith(u'/')) {
        return false;
    }
    // A valid object path has no empty components and no trailing slash.
    if (value.contains(QLatin1String("//"))) {
        return false;
    }
    for (const QChar character : value) {
        const char16_t code = character.unicode();
        const bool allowed = (code >= u'a' && code <= u'z') || (code >= u'A' && code <= u'Z')
            || (code >= u'0' && code <= u'9') || code == u'/' || code == u'_';
        if (!allowed) {
            return false;
        }
    }
    return true;
}

bool isBoundedSafeText(const QString &value, qsizetype maxUtf8Bytes)
{
    if (value.toUtf8().size() > maxUtf8Bytes) {
        return false;
    }
    return !containsForbiddenControl(value);
}

ValidationOutcome validateOwnerKey(const OwnerKey &key)
{
    if (key.generation == 0) {
        return ValidationOutcome::failure(ValidationError::InvalidGeneration,
                                          QStringLiteral("zero-generation"));
    }
    if (!isValidUniqueBusName(key.uniqueName)) {
        return ValidationOutcome::failure(ValidationError::InvalidOwnerName,
                                          QStringLiteral("owner-not-unique-bus-name"));
    }
    if (!isValidObjectPath(key.objectPath)) {
        return ValidationOutcome::failure(ValidationError::InvalidObjectPath,
                                          QStringLiteral("invalid-object-path"));
    }
    return ValidationOutcome::success();
}

ValidationOutcome validatePixmap(const Pixmap &pixmap)
{
    if (pixmap.width == 0 || pixmap.height == 0) {
        return ValidationOutcome::failure(ValidationError::InvalidIcon,
                                          QStringLiteral("pixmap-zero-dimension"));
    }
    if (pixmap.width > kMaxIconPixmapDimension || pixmap.height > kMaxIconPixmapDimension) {
        return ValidationOutcome::failure(ValidationError::InvalidIcon,
                                          QStringLiteral("pixmap-dimension-exceeded"));
    }
    const qsizetype expectedBytes = qsizetype(pixmap.width) * qsizetype(pixmap.height) * 4;
    if (expectedBytes > kMaxIconPixmapBytes) {
        return ValidationOutcome::failure(ValidationError::InvalidIcon,
                                          QStringLiteral("pixmap-budget-exceeded"));
    }
    if (pixmap.argb.size() != expectedBytes) {
        return ValidationOutcome::failure(ValidationError::InvalidIcon,
                                          QStringLiteral("pixmap-byte-count-mismatch"));
    }
    return ValidationOutcome::success();
}

ValidationOutcome validateIconPayload(const IconPayload &icon)
{
    const ValidationOutcome name =
        validateText(icon.iconName, kMaxIconNameUtf8Bytes, ValidationError::InvalidIcon);
    if (!name.accepted) {
        return name;
    }
    const ValidationOutcome attentionName =
        validateText(icon.attentionIconName, kMaxIconNameUtf8Bytes, ValidationError::InvalidIcon);
    if (!attentionName.accepted) {
        return attentionName;
    }
    const ValidationOutcome movie =
        validateText(icon.attentionMovieName, kMaxMovieNameUtf8Bytes, ValidationError::InvalidIcon);
    if (!movie.accepted) {
        return movie;
    }
    return validatePixmapList(icon.pixmaps + icon.attentionPixmaps,
                              kMaxIconPixmaps,
                              ValidationError::InvalidIcon);
}

ValidationOutcome validateToolTip(const ToolTipPayload &toolTip)
{
    const ValidationOutcome icon =
        validateText(toolTip.iconName, kMaxIconNameUtf8Bytes, ValidationError::InvalidToolTip);
    if (!icon.accepted) {
        return icon;
    }
    const ValidationOutcome title =
        validateText(toolTip.title, kMaxTitleUtf8Bytes, ValidationError::InvalidToolTip);
    if (!title.accepted) {
        return title;
    }
    const ValidationOutcome description =
        validateText(toolTip.description, kMaxTextUtf8Bytes, ValidationError::InvalidToolTip);
    if (!description.accepted) {
        return description;
    }
    return validatePixmapList(toolTip.pixmaps, kMaxToolTipPixmaps, ValidationError::InvalidToolTip);
}

namespace
{

ValidationOutcome validateMenuEntry(const MenuEntry &entry, qsizetype index,
                                    const QList<MenuEntry> &entries)
{
    switch (entry.kind) {
    case MenuEntry::Kind::Item:
        if (entry.label.trimmed().isEmpty()) {
            return ValidationOutcome::failure(ValidationError::InvalidMenu,
                                              QStringLiteral("menu-item-unlabeled"));
        }
        break;
    case MenuEntry::Kind::Separator:
        if (!entry.label.isEmpty() || !entry.iconName.isEmpty() || !entry.shortcut.isEmpty()) {
            return ValidationOutcome::failure(ValidationError::InvalidMenu,
                                              QStringLiteral("separator-with-content"));
        }
        break;
    case MenuEntry::Kind::SubMenu:
        if (entry.label.trimmed().isEmpty()) {
            return ValidationOutcome::failure(ValidationError::InvalidMenu,
                                              QStringLiteral("submenu-unlabeled"));
        }
        break;
    default:
        return ValidationOutcome::failure(ValidationError::InvalidMenu,
                                          QStringLiteral("unknown-menu-kind"));
    }

    if (entry.parentId < -1 || entry.parentId >= index) {
        // Parents must exist and be declared earlier; forward references and
        // self-parents would let a hostile payload build a cycle.
        return ValidationOutcome::failure(ValidationError::InvalidMenu,
                                          QStringLiteral("menu-parent-invalid"));
    }

    const ValidationOutcome label =
        validateText(entry.label, kMaxTitleUtf8Bytes, ValidationError::InvalidMenu);
    if (!label.accepted) {
        return label;
    }
    const ValidationOutcome icon =
        validateText(entry.iconName, kMaxIconNameUtf8Bytes, ValidationError::InvalidMenu);
    if (!icon.accepted) {
        return icon;
    }
    const ValidationOutcome shortcut =
        validateText(entry.shortcut, kMaxShortcutUtf8Bytes, ValidationError::InvalidMenu);
    if (!shortcut.accepted) {
        return shortcut;
    }

    if (entry.kind == MenuEntry::Kind::SubMenu) {
        // Children must declare an earlier parent, so a submenu is empty
        // exactly when no later entry claims this index.
        for (qsizetype other = index + 1; other < entries.size(); ++other) {
            if (entries.at(other).parentId == index) {
                return ValidationOutcome::success();
            }
        }
        return ValidationOutcome::failure(ValidationError::InvalidMenu,
                                          QStringLiteral("submenu-empty"));
    }
    return ValidationOutcome::success();
}

} // namespace

ValidationOutcome validateMenu(const MenuPayload &menu)
{
    if (menu.entries.size() > kMaxMenuNodes) {
        return ValidationOutcome::failure(ValidationError::InvalidMenu,
                                          QStringLiteral("menu-node-budget-exceeded"));
    }

    for (qsizetype index = 0; index < menu.entries.size(); ++index) {
        const ValidationOutcome entryOutcome =
            validateMenuEntry(menu.entries.at(index), index, menu.entries);
        if (!entryOutcome.accepted) {
            return entryOutcome;
        }

        // Walk the parent chain with a depth budget; a cycle cannot exist
        // because parents are always earlier entries, but the walk still
        // bounds chain length explicitly instead of trusting that rule.
        qsizetype cursor = menu.entries.at(index).parentId;
        int depth = 1;
        while (cursor >= 0) {
            ++depth;
            if (depth > kMaxMenuDepth) {
                return ValidationOutcome::failure(ValidationError::InvalidMenu,
                                                  QStringLiteral("menu-depth-exceeded"));
            }
            cursor = menu.entries.at(cursor).parentId;
        }
    }
    return ValidationOutcome::success();
}

ValidationOutcome validateItemDescriptor(const ItemDescriptor &descriptor)
{
    switch (descriptor.category) {
    case ItemCategory::Application:
    case ItemCategory::Communications:
    case ItemCategory::SystemServices:
    case ItemCategory::Hardware:
        break;
    default:
        return ValidationOutcome::failure(ValidationError::InvalidCategory,
                                          QStringLiteral("unknown-category"));
    }

    switch (descriptor.status) {
    case ItemStatus::Passive:
    case ItemStatus::Active:
    case ItemStatus::NeedsAttention:
        break;
    default:
        return ValidationOutcome::failure(ValidationError::InvalidStatus,
                                          QStringLiteral("unknown-status"));
    }

    const qsizetype identityBytes = descriptor.identity.toUtf8().size();
    if (identityBytes == 0 || identityBytes > kMaxIdentityUtf8Bytes
        || containsForbiddenControl(descriptor.identity)) {
        return ValidationOutcome::failure(ValidationError::InvalidIdentity,
                                          QStringLiteral("invalid-identity"));
    }
    const ValidationOutcome title =
        validateText(descriptor.title, kMaxTitleUtf8Bytes, ValidationError::InvalidTitle);
    if (!title.accepted) {
        return title;
    }
    const ValidationOutcome icon = validateIconPayload(descriptor.icon);
    if (!icon.accepted) {
        return icon;
    }
    const ValidationOutcome toolTip = validateToolTip(descriptor.toolTip);
    if (!toolTip.accepted) {
        return toolTip;
    }
    return ValidationOutcome::success();
}

} // namespace QindaQt::StatusNotifier
