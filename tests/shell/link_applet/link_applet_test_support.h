// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_client/link_transport.h>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QStringList>

namespace QindaQt::Shell::LinkApplet::TestSupport
{

// Deterministic fake for org.qindaqt.Link1. announceAvailable() is the only
// path that emits serviceAvailableChanged; Devices() answers synchronously
// from `devicesJson`. The fire-and-forget calls are recorded.
class FakeLinkTransport final : public Link::LinkTransport
{
    Q_OBJECT
public:
    [[nodiscard]] bool start(QString *error = nullptr) override
    {
        if (error != nullptr) {
            error->clear();
        }
        running = true;
        return true;
    }
    void stop() override { running = false; }
    void requestDevices(const quint64 token) override
    {
        ++deviceRequests;
        Q_EMIT devicesReceived(token, devicesJson);
    }
    void confirmPairing(const QString &id, const bool accept) override
    {
        confirmations.append(id + (accept ? QStringLiteral(":accept") : QStringLiteral(":reject")));
    }
    void unpair(const QString &id) override { unpaired.append(id); }
    void ring(const QString &id) override { rang.append(id); }

    void announceAvailable(const bool value)
    {
        available = value;
        Q_EMIT serviceAvailableChanged(value);
    }
    void setDevices(const QByteArray &json)
    {
        devicesJson = json;
        Q_EMIT devicesChanged(json);
    }
    void raisePairing(const QString &id, const QString &name, const QString &model,
                      const QString &code)
    {
        Q_EMIT pairingRequested(id, name, model, code);
    }

    bool running = false;
    bool available = false;
    int deviceRequests = 0;
    QByteArray devicesJson = QByteArrayLiteral("[]");
    QStringList confirmations, unpaired, rang;
};

inline QJsonObject deviceObject(const QString &id, const QString &name, bool paired, bool connected,
                                bool ready, int battery = -1, bool charging = false,
                                const QString &transport = QStringLiteral("usb"),
                                const QStringList &caps = {}, qint64 lastSeen = 0)
{
    QJsonObject o{{QStringLiteral("id"), id},
                  {QStringLiteral("name"), name},
                  {QStringLiteral("model"), QStringLiteral("SM-TEST")},
                  {QStringLiteral("paired"), paired},
                  {QStringLiteral("connected"), connected},
                  {QStringLiteral("ready"), ready},
                  {QStringLiteral("transport"), connected ? transport : QStringLiteral("none")},
                  {QStringLiteral("lastSeen"), double(lastSeen)}};
    if (battery >= 0) {
        o.insert(QStringLiteral("battery"),
                 QJsonObject{{QStringLiteral("level"), battery},
                             {QStringLiteral("charging"), charging},
                             {QStringLiteral("plugged"), QStringLiteral("usb")}});
        o.insert(QStringLiteral("network"),
                 QJsonObject{{QStringLiteral("type"), QStringLiteral("wifi")},
                             {QStringLiteral("signal"), 3}});
    }
    if (!caps.isEmpty()) {
        o.insert(QStringLiteral("caps"), QJsonArray::fromStringList(caps));
    }
    return o;
}

inline QByteArray devicesJson(const QList<QJsonObject> &devices)
{
    QJsonArray array;
    for (const auto &d : devices) {
        array.append(d);
    }
    return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

} // namespace QindaQt::Shell::LinkApplet::TestSupport
