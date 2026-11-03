// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_service/setup_adaptor.h>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

namespace QindaQt::Link {
namespace {
QString compact(const QJsonDocument &doc) { return QString::fromUtf8(doc.toJson(QJsonDocument::Compact)); }
}

SetupAdaptor::SetupAdaptor(AdbRunner *runner, TunnelManager *tunnels, QDBusConnection bus, QObject *exported,
                           CallerAuthority *authority)
    : QDBusAbstractAdaptor(exported), m_tunnels(tunnels), m_authority(authority), m_bus(std::move(bus))
{
    if (runner != nullptr) {
        m_setup = new PhoneSetup(*runner, QStringLiteral("org.qindaqt.link"), this);
    }
}

bool SetupAdaptor::allowed(const QDBusMessage &msg)
{
    msg.setDelayedReply(true);
    if (m_authority == nullptr) {
        m_bus.send(msg.createErrorReply(QStringLiteral("org.qindaqt.Link1.Error.Denied"), QStringLiteral("denied")));
        return false;
    }
    return m_authority->authorize(msg);  // queues the Denied reply itself on false
}

void SetupAdaptor::reply(const QDBusMessage &msg, const QString &json) { m_bus.send(msg.createReply(json)); }

void SetupAdaptor::fail(const QDBusMessage &msg, const QString &code)
{
    // Fixed names only: a name built from outside input that is not a valid D-Bus name makes the bus drop the reply.
    m_bus.send(msg.createErrorReply(
        code == QLatin1String("bad-request") ? QStringLiteral("org.qindaqt.Link1.Error.BadRequest")
                                             : QStringLiteral("org.qindaqt.Link1.Error.NotConnected"),
        code));
}

QString SetupAdaptor::AdbDevices(const QDBusMessage &msg)
{
    if (!allowed(msg)) return {};
    QJsonArray out;
    if (m_tunnels != nullptr) {
        for (const TunnelStatus &d : m_tunnels->devices()) {
            out.append(QJsonObject{{QStringLiteral("serial"), d.serial},
                                   {QStringLiteral("appInstalled"), d.appInstalled},
                                   {QStringLiteral("tunneled"), d.tunneled}});
        }
    }
    reply(msg, compact(QJsonDocument(out)));
    return {};
}

QString SetupAdaptor::SetupPhone(const QString &serial, const QDBusMessage &msg)
{
    if (!allowed(msg)) return {};
    if (m_setup == nullptr) {
        fail(msg, QStringLiteral("not-connected"));
    } else if (!PhoneSetup::validSerial(serial)) {
        fail(msg, QStringLiteral("bad-request"));
    } else {
        m_setup->run(serial, [this, msg](const QJsonObject &report) { reply(msg, compact(QJsonDocument(report))); });
    }
    return {};
}

QString SetupAdaptor::SetupStatus(const QString &serial, const QDBusMessage &msg)
{
    if (!allowed(msg)) return {};
    if (m_setup == nullptr) {
        fail(msg, QStringLiteral("not-connected"));
    } else if (!PhoneSetup::validSerial(serial)) {
        fail(msg, QStringLiteral("bad-request"));
    } else {
        m_setup->status(serial, [this, msg](const QJsonObject &report) { reply(msg, compact(QJsonDocument(report))); });
    }
    return {};
}

} // namespace QindaQt::Link
