// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_messages/message_model.h>
#include <qindaqt/services/link_transport/session.h>

#include <QtCore/QJsonObject>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QStringList>

#include <functional>

namespace QindaQt::Link {

// Desktop side of the `sms` capability (protocol page, "Messages (sms.*)").
// Requests go to the phone through Session::request; phone events
// (sms.changed, sms.sent) arrive through handle().
//
// Ownership: borrows the session lookup (normally Hub::session) and never
// keeps a Session pointer past one call. Threading: single thread, the
// sessions' thread. Error behavior: every request completes its Reply exactly
// once, with an empty error string on success, otherwise a protocol error code
// from the phone (denied, unsupported, bad-request, ...) or one of the local
// codes `not-connected`, `unsupported` (peer lacks `sms`), `bad-request`
// (arguments out of bounds), `bad-reply` (phone data failed validation),
// `timeout`.
// AGENT-GUARD: the in-memory model holds counts only (UnreadTotals). Message
// content is handed to the caller and not retained, so it cannot reach disk or logs.
class MessagesFeature final : public QObject, public FeatureHandler {
    Q_OBJECT
public:
    using SessionLookup = std::function<Session *(const QString &deviceId)>;
    using Reply = std::function<void(const QJsonObject &body, const QString &error)>;

    explicit MessagesFeature(SessionLookup lookup, QObject *parent = nullptr);

    [[nodiscard]] QString ns() const override { return QStringLiteral("sms"); }
    void handle(Session &session, const Envelope &envelope) override;
    void sessionClosed(const QString &deviceId) override;

    // `before`/`beforeId` < 0 mean "from the newest". Limits are clamped to the protocol maxima.
    void listThreads(const QString &deviceId, int limit, qint64 before, qint64 beforeId, Reply done);
    // The cursor is the (date, kind, id) of the last row received; `beforeKind` is "sms" or "mms" (empty = "sms").
    void listMessages(const QString &deviceId, qint64 thread, int limit, qint64 before, const QString &beforeKind,
                      qint64 beforeId, Reply done);
    void getPart(const QString &deviceId, qint64 part, int maxEdge, Reply done);
    void send(const QString &deviceId, const QStringList &addrs, const QString &text, int subscription, Reply done);
    void markRead(const QString &deviceId, qint64 thread, Reply done);

    [[nodiscard]] int unreadTotal() const { return m_unread.total(); }

Q_SIGNALS:
    // `json`: {"threads":[ids],"all":bool,"unread":n}
    void threadsChanged(const QString &deviceId, const QString &json);
    // `detail` is "" or {"unsent":[recipient indexes never sent]} for a failure after earlier recipients went out.
    void sendStatus(const QString &deviceId, qint64 ref, const QString &state, const QString &code, const QString &detail);
    void unreadTotalChanged(int total);

private:
    void ask(const QString &deviceId, const QString &type, const QJsonObject &body,
             std::function<std::optional<QJsonObject>(const QJsonObject &)> validate,
             std::function<void(const QJsonObject &)> onOk, Reply done);

    SessionLookup m_lookup;
    UnreadTotals m_unread;
    QHash<QString, QSet<qint64>> m_sendRefs; // refs this desktop issued; bounded, ignores forged events
};

} // namespace QindaQt::Link
