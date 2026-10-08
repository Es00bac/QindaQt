// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/bluetooth_radio_helper/radio_service_session.h>
#include "native_radio_wire_p.h"

namespace QindaQt::BluetoothRadio {
class RadioServiceSession::Private final {
public:
    std::unique_ptr<NativeRadioWire> wire;
    QDBusConnection authority{QStringLiteral("unavailable-radio-authority")};
    QString connectionName;
    QString reason = QStringLiteral("radio-helper-unavailable");
    bool ready = false;
    bool portAttached = false;
    bool fallbackAllowed = true;
};
} // namespace QindaQt::BluetoothRadio
