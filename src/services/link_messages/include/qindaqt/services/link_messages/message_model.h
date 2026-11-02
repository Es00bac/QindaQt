// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QHash>
#include <QtCore/QJsonObject>
#include <QtCore/QString>

#include <optional>

namespace QindaQt::Link {

// Validation of phone-supplied `sms.*` bodies plus the in-memory unread
// totals. Validators rebuild the object from known keys only and enforce the
// protocol page's bounds; they return nullopt for anything malformed, so a
// buggy or hostile phone cannot push oversized or mistyped data to D-Bus
// clients.
//
// AGENT-GUARD: message content (bodies, addresses, names, snippets) exists only in
// objects passed through here and in D-Bus replies. Nothing in this module
// writes it to disk or to a log; keep it that way.
// AGENT-CONTRACT: bounds mirror docs/wiki/reference/qindalink-protocol-v1.md,
// "Messages (sms.*)"; the phone side is org.qindaqt.link.sms.SmsModel.
namespace MessageBounds {
inline constexpr int kMaxThreads = 100;
inline constexpr int kMaxMessages = 200;
inline constexpr int kMaxSendAddrs = 10;
inline constexpr int kMaxSendText = 1600;
inline constexpr qsizetype kMaxPartBase64 = 3 * 1024 * 1024;
} // namespace MessageBounds

[[nodiscard]] std::optional<QJsonObject> sanitizeThreadsReply(const QJsonObject &body);
[[nodiscard]] std::optional<QJsonObject> sanitizeMessagesReply(const QJsonObject &body);
[[nodiscard]] std::optional<QJsonObject> sanitizePartReply(const QJsonObject &body);
[[nodiscard]] std::optional<QJsonObject> sanitizeSendReply(const QJsonObject &body);
[[nodiscard]] std::optional<QJsonObject> sanitizeReadReply(const QJsonObject &body);
[[nodiscard]] std::optional<QJsonObject> sanitizeChanged(const QJsonObject &body);
[[nodiscard]] std::optional<QJsonObject> sanitizeSentEvent(const QJsonObject &body);

// Unread message totals reported by connected phones. Single-threaded.
class UnreadTotals {
public:
    // Returns true when the grand total changed.
    bool set(const QString &deviceId, int unread);
    bool remove(const QString &deviceId);
    [[nodiscard]] int total() const;
    [[nodiscard]] int of(const QString &deviceId) const { return m_unread.value(deviceId, 0); }

private:
    QHash<QString, int> m_unread;
};

} // namespace QindaQt::Link
