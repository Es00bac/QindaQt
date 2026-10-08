// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QtCore/QString>
#include <QtDBus/QDBusConnection>
#include <memory>

namespace QindaQt::BluetoothRadio {
class QtRadioPowerPort;
// Owns a native caller and Qt authority connection to one GUID-pinned Unix bus.
// Construct/use/destroy on one Qt thread, before the resident service and port;
// this object must outlive both. The address is captured once by composition.
// No radio request or helper activation occurs here. Failure before selecting
// any peer permits existing Qt-only service startup via legacyStartupAllowed();
// selected GUID mismatch/loss never permits fallback, reconnect or rebinding.
// New pre-deployment helper SDK; Bluetooth1/2 compatibility is unchanged.
class RadioServiceSession final {
public:
    explicit RadioServiceSession(const QString &constructingAddress);
    ~RadioServiceSession();
    RadioServiceSession(const RadioServiceSession &) = delete;
    RadioServiceSession &operator=(const RadioServiceSession &) = delete;
    [[nodiscard]] bool prepared() const;
    [[nodiscard]] bool legacyStartupAllowed() const;
    [[nodiscard]] QDBusConnection authorityConnection() const;
    [[nodiscard]] QString reasonCode() const;
private:
    friend class QtRadioPowerPort; // Same owning module; no native types in SDK.
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace QindaQt::BluetoothRadio
