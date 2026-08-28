// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/status_notifier/status_notifier_limits.h>
#include <qindaqt/shell/status_notifier/status_notifier_types.h>

#include <QtCore/QString>

namespace QindaQt::StatusNotifier
{

enum class ValidationError : quint32 {
    None = 0,
    InvalidOwnerName = 1,
    InvalidObjectPath = 2,
    InvalidGeneration = 3,
    InvalidIdentity = 4,
    InvalidTitle = 5,
    InvalidText = 6,
    InvalidIcon = 7,
    InvalidToolTip = 8,
    InvalidMenu = 9,
    InvalidCategory = 10,
    InvalidStatus = 11,
};

struct ValidationOutcome {
    bool accepted = false;
    ValidationError error = ValidationError::None;
    QString reasonCode;

    [[nodiscard]] static ValidationOutcome success();
    [[nodiscard]] static ValidationOutcome failure(ValidationError error, QString reasonCode);
};

// D-Bus unique bus names are ":<digits>.<digits>" with at least one digit on
// each side. Well-known names lack the leading colon; rejecting them here is
// the first spoofed-owner defense.
[[nodiscard]] bool isValidUniqueBusName(const QString &value);
[[nodiscard]] bool isValidObjectPath(const QString &value);

// Rejects text beyond `maxUtf8Bytes` and any embedded NUL or C0 control
// character; presentation text must never carry raw control bytes.
[[nodiscard]] bool isBoundedSafeText(const QString &value, qsizetype maxUtf8Bytes);

[[nodiscard]] ValidationOutcome validateOwnerKey(const OwnerKey &key);
[[nodiscard]] ValidationOutcome validatePixmap(const Pixmap &pixmap);
[[nodiscard]] ValidationOutcome validateIconPayload(const IconPayload &icon);
[[nodiscard]] ValidationOutcome validateToolTip(const ToolTipPayload &toolTip);
// Fails closed on more than kMaxMenuNodes entries, parent chains deeper than
// kMaxMenuDepth, parent cycles, unknown parent indices, separators carrying
// content, empty submenus, or unlabeled items/submenus.
[[nodiscard]] ValidationOutcome validateMenu(const MenuPayload &menu);
[[nodiscard]] ValidationOutcome validateItemDescriptor(const ItemDescriptor &descriptor);

} // namespace QindaQt::StatusNotifier
