// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/bluetooth_radio_helper/radio_types.h>
#include <memory>
#include <functional>

namespace QindaQt::BluetoothRadio {
struct RadioObservation {
    bool current = false;
    bool softBlocked = false;
    bool hardBlocked = false;
};
enum class RadioWrite { NotAttempted, Denied, Attempted };

// One operation owns its selection and descriptors. The kernel implementation
// never accepts an rfkill index from IPC. A retired selection cannot rebind.
class RadioLease {
public:
    virtual ~RadioLease() = default;
    virtual RadioObservation observe() = 0;
    virtual RadioWrite unblock(const std::function<bool()> &current) = 0;
};
struct RadioSelection {
    std::unique_ptr<RadioLease> lease;
    QString reasonCode;
};
class RadioPlatform {
public:
    virtual ~RadioPlatform() = default;
    virtual RadioSelection select(const QString &adapterPath) = 0;
};
std::unique_ptr<RadioPlatform> makeLinuxRadioPlatform();
} // namespace QindaQt::BluetoothRadio
