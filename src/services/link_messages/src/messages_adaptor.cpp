// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_messages/messages_adaptor.h>

#include <QtCore/QJsonDocument>
#include <QtCore/QVariantMap>

namespace QindaQt::Link {
namespace {

// AGENT-GUARD: the code comes from the phone (or a local failure). Only known codes may become part of a D-Bus
// error name: an invalid name makes the bus drop the reply and the caller hangs until its timeout.
QString errorName(const QString &code)
{
    static const QStringList known = {
        QStringLiteral("unknown-type"), QStringLiteral("bad-request"), QStringLiteral("not-paired"),
        QStringLiteral("unsupported"), QStringLiteral("busy"), QStringLiteral("denied"),
        QStringLiteral("target-changed"), QStringLiteral("timeout"), QStringLiteral("internal"),
        QStringLiteral("not-connected"), QStringLiteral("bad-reply")};
    if (!known.contains(code)) return QStringLiteral("org.qindaqt.Link1.Error.Internal");
    QString out = QStringLiteral("org.qindaqt.Link1.Error.");
    bool up = true;
    for (const QChar c : code) {
        if (c == QLatin1Char('-')) {
            up = true;
        } else {
            out += up ? c.toUpper() : c;
            up = false;
        }
    }
    return out;
}

} // namespace

MessagesAdaptor::MessagesAdaptor(MessagesFeature *feature, QDBusConnection bus)
    : QDBusAbstractAdaptor(feature), m_feature(feature), m_bus(std::move(bus))
{
    connect(feature, &MessagesFeature::threadsChanged, this, &MessagesAdaptor::ThreadsChanged);
    connect(feature, &MessagesFeature::sendStatus, this, &MessagesAdaptor::SendStatus);
    connect(feature, &MessagesFeature::unreadTotalChanged, this, [this](int total) {
        QDBusMessage sig = QDBusMessage::createSignal(QStringLiteral("/org/qindaqt/Link1/Messages"),
                                                      QStringLiteral("org.freedesktop.DBus.Properties"),
                                                      QStringLiteral("PropertiesChanged"));
        sig << QStringLiteral("org.qindaqt.Link1.Messages") << QVariantMap{{QStringLiteral("UnreadCount"), total}}
            << QStringList();
        m_bus.send(sig);
    });
}

MessagesFeature::Reply MessagesAdaptor::completer(const QDBusMessage &msg)
{
    msg.setDelayedReply(true);
    return [bus = m_bus, msg](const QJsonObject &body, const QString &error) mutable {
        if (error.isEmpty()) {
            bus.send(msg.createReply(QString::fromUtf8(QJsonDocument(body).toJson(QJsonDocument::Compact))));
        } else {
            bus.send(msg.createErrorReply(errorName(error), error));
        }
    };
}

QString MessagesAdaptor::ListThreads(const QString &device, int limit, qlonglong before, qlonglong beforeId,
                                     const QDBusMessage &msg)
{
    m_feature->listThreads(device, limit, before, beforeId, completer(msg));
    return {};
}

QString MessagesAdaptor::ListMessages(const QString &device, qlonglong thread, int limit, qlonglong before,
                                      const QString &beforeKind, qlonglong beforeId, const QDBusMessage &msg)
{
    m_feature->listMessages(device, thread, limit, before, beforeKind, beforeId, completer(msg));
    return {};
}

QString MessagesAdaptor::GetPart(const QString &device, qlonglong part, int maxEdge, const QDBusMessage &msg)
{
    m_feature->getPart(device, part, maxEdge, completer(msg));
    return {};
}

QString MessagesAdaptor::Send(const QString &device, const QStringList &addrs, const QString &text, int subscription,
                              const QDBusMessage &msg)
{
    m_feature->send(device, addrs, text, subscription, completer(msg));
    return {};
}

QString MessagesAdaptor::MarkRead(const QString &device, qlonglong thread, const QDBusMessage &msg)
{
    m_feature->markRead(device, thread, completer(msg));
    return {};
}

} // namespace QindaQt::Link
