// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_messages/messages_feature.h>

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
// BadRequest, BadReply, Timeout, Internal, ...). All values are compact JSON text.
// AGENT-GUARD: never log method arguments or reply bodies here; they are message content.
class MessagesAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Link1.Messages")
    Q_PROPERTY(int UnreadCount READ unreadCount)
public:
    MessagesAdaptor(MessagesFeature *feature, QDBusConnection bus);
    [[nodiscard]] int unreadCount() const { return m_feature->unreadTotal(); }

public Q_SLOTS:
    QString ListThreads(const QString &device, int limit, qlonglong before, qlonglong beforeId, const QDBusMessage &msg);
    QString ListMessages(const QString &device, qlonglong thread, int limit, qlonglong before, qlonglong beforeId,
                         const QDBusMessage &msg);
    QString GetPart(const QString &device, qlonglong part, int maxEdge, const QDBusMessage &msg);
    QString Send(const QString &device, const QStringList &addrs, const QString &text, int subscription,
                 const QDBusMessage &msg);
    QString MarkRead(const QString &device, qlonglong thread, const QDBusMessage &msg);

Q_SIGNALS:
    void ThreadsChanged(const QString &device, const QString &json);
    void SendStatus(const QString &device, qlonglong ref, const QString &state, const QString &code);

private:
    [[nodiscard]] MessagesFeature::Reply completer(const QDBusMessage &msg);

    MessagesFeature *m_feature;
    QDBusConnection m_bus;
};

} // namespace QindaQt::Link
