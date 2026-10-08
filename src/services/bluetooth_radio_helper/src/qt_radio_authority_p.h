// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "radio_operation_p.h"
#include <QtDBus/QDBusConnection>

namespace QindaQt::BluetoothRadio {
// Helper-only admission over injected constructing and system buses. Neither
// sender strings nor Adapter1 properties are accepted without daemon ownership.
class QtRadioAuthority final : public RadioAuthority {
public:
    QtRadioAuthority(QDBusConnection session, QDBusConnection bluez);
    bool current(const QString &sender, const Request &request) override;
private:
    QDBusConnection m_session, m_bluez;
};
} // namespace QindaQt::BluetoothRadio
