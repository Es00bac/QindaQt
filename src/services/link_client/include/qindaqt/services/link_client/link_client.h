// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_client/link_transport.h>

#include <QtCore/QList>

namespace QindaQt::Link {

struct LinkDevice {
    QString id, name, model, transport;  // transport: usb | lan | none
    bool paired = false, connected = false, ready = false, charging = false;
    int batteryLevel = -1;               // -1 = unknown
    QString plugged, networkType;
    int signal = -1;                     // -1 = unknown
    qint64 lastSeen = 0;                 // ms since epoch, 0 = never
};

// Parses a Devices() / DevicesChanged JSON array. Malformed entries are skipped.
[[nodiscard]] QList<LinkDevice> parseDevices(const QByteArray &json);

// Typed model over Link1 for the applet and the app.
// Ownership: borrows `transport`, which must outlive the client.
class LinkClient final : public QObject {
    Q_OBJECT
public:
    explicit LinkClient(LinkTransport &transport, QObject *parent = nullptr);
    [[nodiscard]] bool start(QString *error = nullptr);
    void stop();
    [[nodiscard]] bool available() const { return m_available; }
    [[nodiscard]] const QList<LinkDevice> &devices() const { return m_devices; }

    void confirmPairing(const QString &id, bool accept) { m_transport.confirmPairing(id, accept); }
    void unpair(const QString &id) { m_transport.unpair(id); }
    void ring(const QString &id) { m_transport.ring(id); }

Q_SIGNALS:
    void availableChanged();
    void devicesChanged();
    void pairingRequested(const QString &id, const QString &name, const QString &model, const QString &code);

private:
    void setDevices(const QByteArray &json);

    LinkTransport &m_transport;
    QList<LinkDevice> m_devices;
    bool m_available = false;
    quint64 m_nextToken = 1;
    quint64 m_pendingToken = 0;
};

} // namespace QindaQt::Link
