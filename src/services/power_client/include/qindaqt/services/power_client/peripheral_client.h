// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/services/power_client/peripheral_transport.h>
#include <qindaqt/services/power_protocol/peripheral_types.h>
#include <QtCore/QTimer>
namespace QindaQt::Power {
// Read-only sibling domain on the same Qt thread. Borrowed authority and
// transport must outlive this client. Owner/epoch loss clears rows; a receipt
// can publish only for its current request token and exact main-client epoch.
class PeripheralClient final : public QObject {
    Q_OBJECT
public:
    PeripheralClient(PowerClient *authority,PeripheralTransport *transport,QObject *parent=nullptr);
    ~PeripheralClient() override;
    void start();
    void stop();
    void refresh();
    [[nodiscard]] PeripheralSnapshot snapshot() const { return m_snapshot; }
    [[nodiscard]] bool hasSnapshot() const { return m_valid; }
    [[nodiscard]] QString reasonCode() const { return m_reason; }
Q_SIGNALS:
    void changed();
private:
    void synchronize();
    void accept(const QString &owner,quint64 token,const QByteArray &payload);
    void clear(const QString &reason);
    PowerClient *m_authority;
    PeripheralTransport *m_transport;
    QTimer m_timeout;
    PeripheralSnapshot m_snapshot;
    QString m_owner,m_reason;
    quint64 m_epoch=0,m_nextToken=0,m_pending=0,m_minRevision=0;
    bool m_running=false,m_valid=false,m_dirty=false;
};
}
