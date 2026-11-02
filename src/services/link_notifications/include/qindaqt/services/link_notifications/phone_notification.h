// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_protocol/envelope.h>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QString>

namespace QindaQt::Link {

// Bounds from docs/wiki/reference/qindalink-protocol-v1.md, "Notifications".
// AGENT-CONTRACT: the phone (org/qindaqt/link/notify/NotifyFrames.java) uses
// the same numbers. Lengths are UTF-16 code units, which is QString::size().
namespace NotifyLimits {
inline constexpr qsizetype kKey = 512;
inline constexpr qsizetype kPackage = 256;
inline constexpr qsizetype kApp = 128;
inline constexpr qsizetype kTitle = 256;
inline constexpr qsizetype kText = 2048;
inline constexpr qsizetype kActions = 8;
inline constexpr qsizetype kActionId = 64;
inline constexpr qsizetype kActionTitle = 64;
inline constexpr qsizetype kReplyText = 4096;
inline constexpr qsizetype kSnapshotItems = 50;
inline constexpr qsizetype kIconBase64 = 65'536;
inline constexpr qsizetype kIconBytes = 49'152;
} // namespace NotifyLimits

struct PhoneAction {
    QString id;
    QString title;
    bool reply = false;
    [[nodiscard]] bool operator==(const PhoneAction &) const = default;
};

// One mirrored phone notification after validation. Title and text are
// personal content: never log them.
struct PhoneNotification {
    QString key;
    QString pkg;
    QString app;
    QString title;
    QString text;
    qint64 when = 0;
    QString icon;  // 64 lowercase hex or empty
    QList<PhoneAction> actions;
    [[nodiscard]] bool operator==(const PhoneNotification &) const = default;
    [[nodiscard]] bool hasReply() const;
    [[nodiscard]] QJsonObject toJson() const;
};

[[nodiscard]] bool isIconHash(const QString &s);

// Strict validation of one notification object. False on any violation.
[[nodiscard]] bool parseNotification(const QJsonObject &o, PhoneNotification *out);
// `items` of a snapshot; false when oversized or any item is invalid.
[[nodiscard]] bool parseSnapshot(const QJsonObject &b, QList<PhoneNotification> *out);

[[nodiscard]] Envelope makeDismiss(const QString &key);
[[nodiscard]] Envelope makeAction(const QString &key, const QString &actionId, const QString &replyText,
                                  bool hasText);
[[nodiscard]] Envelope makeIconRequest(const QString &hash);

} // namespace QindaQt::Link
