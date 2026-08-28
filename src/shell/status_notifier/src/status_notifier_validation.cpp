// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/status_notifier/status_notifier_validation.h>

namespace QindaQt::StatusNotifier
{
namespace
{

bool containsForbiddenControl(const QString &value)
{
    // NUL, C0, DEL, and C1 control characters are never meaningful
    // presentation text; screen readers and painters can render them as
    // garbage, and NUL can truncate downstream C-based consumers.
    for (const QChar character : value) {
        const char16_t code = character.unicode();
        if (code <= 0x001F || (code >= 0x007F && code <= 0x009F)) {
            return true;
        }
    }
    return false;
}

ValidationOutcome validateText(const QString &value, qsizetype maxUtf8Bytes, ValidationError error)
{
    if (!isBoundedSafeText(value, maxUtf8Bytes)) {
        return ValidationOutcome::failure(error, QStringLiteral("unbounded-or-control-text"));
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
    if (length < 4 || length > kMaxUniqueNameUtf8Bytes) {
        return false;
    }
    if (!value.startsWith(u':')) {
        return false;
    }

    qsizetype elementStart = 1;
    qsizetype dotCount = 0;

    for (qsizetype index = 1; index < length; ++index) {
        const QChar character = value.at(index);
        if (character == u'.') {
            if (index == elementStart) {
                // Empty element (e.g. leading dot ":.x", consecutive dots "..", etc.)
                return false;
            }
            ++dotCount;
            elementStart = index + 1;
        } else {
            const char16_t code = character.unicode();
            const bool allowed = (code >= u'a' && code <= u'z')
                || (code >= u'A' && code <= u'Z')
                || (code >= u'0' && code <= u'9')
                || code == u'_' || code == u'-';
            if (!allowed) {
                return false;
            }
        }
    }

    if (elementStart == length) {
        // Trailing dot (e.g. ":1.42.")
        return false;
    }

    return dotCount >= 1;
}

bool isValidObjectPath(const QString &value)
{
    const qsizetype length = value.size();
    if (length < 1 || length > kMaxObjectPathUtf8Bytes) {
        return false;
    }
    if (!value.startsWith(u'/')) {
        return false;
    }
    if (length == 1) {
        return true;
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

bool isAcceptableOptionalText(const QString &value, qsizetype maxUtf8Bytes)
{
    if (value.isEmpty()) {
        return true;
    }
    return isBoundedSafeText(value, maxUtf8Bytes) && !value.trimmed().isEmpty();
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

    // AGENT-GUARD: Validate aggregate counts before iterating; do not concatenate lists,
    // which would allocate and copy hostile payload vectors before refusal.
    const qsizetype pixmapsCount = icon.pixmaps.size();
    const qsizetype attentionPixmapsCount = icon.attentionPixmaps.size();
    if (pixmapsCount < 0 || attentionPixmapsCount < 0
        || pixmapsCount > kMaxIconPixmaps
        || attentionPixmapsCount > (kMaxIconPixmaps - pixmapsCount)) {
        return ValidationOutcome::failure(ValidationError::InvalidIcon,
                                          QStringLiteral("pixmap-count-exceeded"));
    }

    for (const Pixmap &pixmap : icon.pixmaps) {
        const ValidationOutcome outcome = validatePixmap(pixmap);
        if (!outcome.accepted) {
            return outcome;
        }
    }
    for (const Pixmap &pixmap : icon.attentionPixmaps) {
        const ValidationOutcome outcome = validatePixmap(pixmap);
        if (!outcome.accepted) {
            return outcome;
        }
    }

    return ValidationOutcome::success();
}

ValidationOutcome validateToolTip(const ToolTipPayload &toolTip)
{
    const ValidationOutcome icon =
        validateText(toolTip.iconName, kMaxIconNameUtf8Bytes, ValidationError::InvalidToolTip);
    if (!icon.accepted) {
        return icon;
    }
    if (!isAcceptableOptionalText(toolTip.title, kMaxTitleUtf8Bytes)) {
        return ValidationOutcome::failure(ValidationError::InvalidToolTip,
                                          QStringLiteral("blank-or-control-text"));
    }
    if (!isAcceptableOptionalText(toolTip.description, kMaxTextUtf8Bytes)) {
        return ValidationOutcome::failure(ValidationError::InvalidToolTip,
                                          QStringLiteral("blank-or-control-text"));
    }

    if (toolTip.pixmaps.size() > kMaxToolTipPixmaps) {
        return ValidationOutcome::failure(ValidationError::InvalidToolTip,
                                          QStringLiteral("pixmap-count-exceeded"));
    }
    for (const Pixmap &pixmap : toolTip.pixmaps) {
        const ValidationOutcome outcome = validatePixmap(pixmap);
        if (!outcome.accepted) {
            return outcome;
        }
    }
    return ValidationOutcome::success();
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
    if (entry.parentId >= 0
        && entries.at(entry.parentId).kind != MenuEntry::Kind::SubMenu) {
        // Only submenus may hold children; nesting beneath an ordinary item
        // or separator has no presentation meaning and is refused outright.
        return ValidationOutcome::failure(ValidationError::InvalidMenu,
                                          QStringLiteral("menu-parent-not-submenu"));
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

    // AGENT-GUARD: This function is the single admission gate for descriptor
    // values; every payload — including the menu — must be validated here
    // before the registry can make any part of a descriptor visible. A new
    // ItemDescriptor member added without a check below is a fail-open
    // defect of exactly the class Shannon's P1-1 finding describes.
    const qsizetype identityBytes = descriptor.identity.toUtf8().size();
    if (identityBytes == 0 || identityBytes > kMaxIdentityUtf8Bytes
        || containsForbiddenControl(descriptor.identity)
        || descriptor.identity.trimmed().isEmpty()) {
        return ValidationOutcome::failure(ValidationError::InvalidIdentity,
                                          QStringLiteral("invalid-identity"));
    }
    if (!isAcceptableOptionalText(descriptor.title, kMaxTitleUtf8Bytes)) {
        // An absent title falls back to the identity for accessibility; a
        // whitespace-only title would erase the accessible name instead.
        return ValidationOutcome::failure(ValidationError::InvalidTitle,
                                          QStringLiteral("blank-or-control-text"));
    }
    const ValidationOutcome icon = validateIconPayload(descriptor.icon);
    if (!icon.accepted) {
        return icon;
    }
    const ValidationOutcome toolTip = validateToolTip(descriptor.toolTip);
    if (!toolTip.accepted) {
        return toolTip;
    }
    const ValidationOutcome menu = validateMenu(descriptor.menu);
    if (!menu.accepted) {
        return menu;
    }
    return ValidationOutcome::success();
}

} // namespace QindaQt::StatusNotifier
