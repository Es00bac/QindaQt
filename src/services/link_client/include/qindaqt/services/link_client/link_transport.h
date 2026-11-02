// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QString>

namespace QindaQt::Link {

// Thread-confined asynchronous seam for `org.qindaqt.Link1`
// (docs/wiki/reference/link1-v1.md). The production implementation watches
// the well-known name on the session bus; tests inject a fake.
//
// AGENT-CONTRACT: implementations emit serviceAvailableChanged(false) before
// any other signal when the name stops being owned, and never deliver a
// devices reply for a request made before the latest transition to true.
class LinkTransport : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    [[nodiscard]] virtual bool start(QString *error = nullptr) = 0;
    virtual void stop() = 0;
    virtual void requestDevices(quint64 token) = 0;
    // Fire-and-forget: failures are diagnostics, not UI-visible outcomes.
    virtual void confirmPairing(const QString &id, bool accept) = 0;
    virtual void unpair(const QString &id) = 0;
    virtual void ring(const QString &id) = 0;

Q_SIGNALS:
    void serviceAvailableChanged(bool available);
    void devicesReceived(quint64 token, const QByteArray &json);
    void devicesChanged(const QByteArray &json);
    void pairingRequested(const QString &id, const QString &name, const QString &model, const QString &code);
    void requestFailed(quint64 token, const QString &reason);
};

} // namespace QindaQt::Link
