// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_client/link_client.h>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace QindaQt::Link {

QList<LinkDevice> parseDevices(const QByteArray &json)
{
    QList<LinkDevice> out;
    for (const QJsonValue &v : QJsonDocument::fromJson(json).array()) {
        const QJsonObject o = v.toObject();
        LinkDevice d;
        d.id = o.value(QStringLiteral("id")).toString();
        if (d.id.isEmpty()) {
            continue;
        }
        d.name = o.value(QStringLiteral("name")).toString();
        d.model = o.value(QStringLiteral("model")).toString();
        d.transport = o.value(QStringLiteral("transport")).toString(QStringLiteral("none"));
        d.paired = o.value(QStringLiteral("paired")).toBool();
        d.connected = o.value(QStringLiteral("connected")).toBool();
        d.ready = o.value(QStringLiteral("ready")).toBool();
        d.lastSeen = qint64(o.value(QStringLiteral("lastSeen")).toDouble());
        for (const QJsonValue &cap : o.value(QStringLiteral("caps")).toArray()) {
            if (cap.isString()) {
                d.capabilities.append(cap.toString());
            }
        }
        const QJsonObject battery = o.value(QStringLiteral("battery")).toObject();
        d.batteryLevel = battery.contains(QStringLiteral("level")) ? battery.value(QStringLiteral("level")).toInt(-1) : -1;
        d.charging = battery.value(QStringLiteral("charging")).toBool();
        d.plugged = battery.value(QStringLiteral("plugged")).toString();
        const QJsonObject network = o.value(QStringLiteral("network")).toObject();
        d.networkType = network.value(QStringLiteral("type")).toString();
        d.signal = network.value(QStringLiteral("signal")).isDouble() ? network.value(QStringLiteral("signal")).toInt(-1) : -1;
        out.append(d);
    }
    return out;
}

LinkClient::LinkClient(LinkTransport &transport, QObject *parent) : QObject(parent), m_transport(transport)
{
    connect(&m_transport, &LinkTransport::serviceAvailableChanged, this, [this](bool available) {
        m_available = available;
        if (!available) {
            m_pendingToken = 0;
            m_devices.clear();
            Q_EMIT devicesChanged();
        } else {
            m_pendingToken = m_nextToken++;
            m_transport.requestDevices(m_pendingToken);
        }
        Q_EMIT availableChanged();
    });
    connect(&m_transport, &LinkTransport::devicesReceived, this, [this](quint64 token, const QByteArray &json) {
        if (token == m_pendingToken) {
            setDevices(json);
        }
    });
    connect(&m_transport, &LinkTransport::devicesChanged, this, &LinkClient::setDevices);
    connect(&m_transport, &LinkTransport::pairingRequested, this, &LinkClient::pairingRequested);
}

bool LinkClient::start(QString *error) { return m_transport.start(error); }
void LinkClient::stop() { m_transport.stop(); }

void LinkClient::setDevices(const QByteArray &json)
{
    m_devices = parseDevices(json);
    Q_EMIT devicesChanged();
}

} // namespace QindaQt::Link
