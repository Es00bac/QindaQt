// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/bluetooth_radio_helper/radio_power_port.h>
#include <QtDBus/QDBusConnection>
#include <memory>

namespace QindaQt::BluetoothRadio {
// Uses only the injected session bus. Helper activation is attempted only for
// an explicit observeAndUnblock call, once, never on construction or retry.
// Exact helper owner and nonce bind every reply; owner loss/timeout after the
// mutation RPC was sent is Uncertain. No device descriptor crosses this API.
class QtRadioPowerPort final : public RadioPowerPort {
public:
    explicit QtRadioPowerPort(QDBusConnection connection, QObject *parent = nullptr);
    ~QtRadioPowerPort() override;
    quint64 observeAndUnblock(const QString &bluezOwner, const QString &adapterPath,
        const QString &adapterAddress, const QString &initiatingCaller,
        std::function<bool()> current) override;
    void cancel(quint64 id) override;
private:
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace QindaQt::BluetoothRadio
