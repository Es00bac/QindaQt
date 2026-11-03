// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_messages/messages_feature.h>
#include <qindaqt/services/link_messages/send_gate.h>
#include <qindaqt/services/link_transport/caller_authority.h>

#include <QtCore/QStringList>
#include <QtDBus/QDBusAbstractAdaptor>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

namespace QindaQt::Link {

// org.qindaqt.Link1.Messages (docs/wiki/reference/link1-v1.md, "Messages").
// Registered by the owner at /org/qindaqt/Link1/Messages with
// QDBusConnection::ExportAdaptors on the MessagesFeature object.
//
// Replies are asynchronous (QDBusMessage::setDelayedReply): the phone answers
// later. Failures are D-Bus errors named org.qindaqt.Link1.Error.<Code> where
// <Code> is the CamelCase protocol code (Denied, Unsupported, NotConnected,
// BadRequest, BadReply, Timeout, Internal, ...; any unlisted phone code becomes Internal). All values are compact JSON text.
// Access (ADR-0438): ListThreads, ListMessages, GetPart, Send and MarkRead need a trusted caller
// (CallerAuthority) and fail with Error.Denied otherwise; the UnreadCount property and the
// ThreadsChanged / SendStatus signals carry ids and counts only and stay open.
// AGENT-GUARD: never log method arguments or reply bodies here; they are message content.
class MessagesAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Link1.Messages")
    Q_PROPERTY(int UnreadCount READ unreadCount)
public:
    // `authority` is required (a null one denies everything); `gate` may be null (no extra admission control).
    MessagesAdaptor(MessagesFeature *feature, QDBusConnection bus, CallerAuthority *authority, SendGate *gate = nullptr);
    [[nodiscard]] int unreadCount() const { return m_feature->unreadTotal(); }

public Q_SLOTS:
    QString ListThreads(const QString &device, int limit, qlonglong before, qlonglong beforeId, const QDBusMessage &msg);
    QString ListMessages(const QString &device, qlonglong thread, int limit, qlonglong before, const QString &beforeKind,
                         qlonglong beforeId, const QDBusMessage &msg);
    QString GetPart(const QString &device, qlonglong part, int maxEdge, const QDBusMessage &msg);
    QString Send(const QString &device, const QStringList &addrs, const QString &text, int subscription,
                 const QDBusMessage &msg);
    QString MarkRead(const QString &device, qlonglong thread, const QDBusMessage &msg);

Q_SIGNALS:
    void ThreadsChanged(const QString &device, const QString &json);
    void SendStatus(const QString &device, qlonglong ref, const QString &state, const QString &code, const QString &detail);

private:
    [[nodiscard]] MessagesFeature::Reply completer(const QDBusMessage &msg);
    void deny(const QDBusMessage &msg);  // fail-closed reply when no authority was injected

    MessagesFeature *m_feature;
    QDBusConnection m_bus;
    CallerAuthority *m_authority;
    SendGate *m_gate;
};

} // namespace QindaQt::Link
