// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_client/bluetooth_transport.h>

#include <QtDBus/QDBusConnection>

#include <memory>

class QDBusServiceWatcher;

namespace QindaQt::Bluetooth
{

// QtDBus transport for Bluetooth1. The connection is held by value, so the
// caller only has to keep the referenced bus connection object registered with
// Qt D-Bus for the lifetime of this transport; no borrowed reference survives
// construction.
class QtBluetoothTransport : public BluetoothTransport
{
    Q_OBJECT

public:
    explicit QtBluetoothTransport(const QDBusConnection &connection, QString serviceName = {},
                                  QObject *parent = nullptr);
    ~QtBluetoothTransport() override;

    void start() override;
    void stop() override;
    void fetchSnapshot(const QString &owner, quint64 requestId) override;
    void submitOperation(const QString &owner, quint64 requestId,
                         const OperationRequest &request) override;

private:
    void queryInitialOwner();
    void activateService();
    void setOwner(const QString &owner);

    struct Private;
    std::unique_ptr<Private> d;

private Q_SLOTS:
    void onServiceOwnerChanged(const QString &service, const QString &oldOwner,
                               const QString &newOwner);
    void onChanged(quint64 epoch, quint64 revision);
};

} // namespace QindaQt::Bluetooth
