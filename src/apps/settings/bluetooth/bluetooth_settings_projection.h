// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_protocol/bluetooth_types.h>

#include <QtCore/QString>

namespace QindaQt::Apps::SettingsBluetooth::Projection {

[[nodiscard]] QString adapterRowId(const QindaQt::Bluetooth::Handle &handle);
[[nodiscard]] QString deviceRowId(const QindaQt::Bluetooth::Handle &handle);
[[nodiscard]] QString deviceClassLabel(QindaQt::Bluetooth::DeviceClass value);
[[nodiscard]] QString deviceIconName(QindaQt::Bluetooth::DeviceClass value);
[[nodiscard]] QString deviceIconText(QindaQt::Bluetooth::DeviceClass value);
[[nodiscard]] QString boundedNonAddressName(const QString &name,
                                            const QString &fallback);

} // namespace QindaQt::Apps::SettingsBluetooth::Projection
