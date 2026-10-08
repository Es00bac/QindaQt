// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_bluetooth/bluetooth_settings_model.h>

namespace QindaQt::Apps::SettingsBluetooth {
using namespace QindaQt::Bluetooth;
QString BluetoothSettingsModel::failureText(const OperationResult &result) const {
    // Fixed public reason codes only; never render upstream error/device text.
    if (result.kind == OperationKind::SetAdapterPower && result.reasonCode == QLatin1String("radio-hardware-blocked"))
        return tr("Bluetooth is blocked by a hardware switch. Turn the switch on and try again.");
    if (result.kind == OperationKind::SetAdapterPower && result.reasonCode == QLatin1String("radio-software-blocked"))
        return tr("Bluetooth is software-blocked and could not be enabled.");
    if (result.kind == OperationKind::SetAdapterPower && result.reasonCode == QLatin1String("radio-blocked"))
        return tr("The Bluetooth adapter is blocked. Check its hardware and software radio controls.");
    if (result.kind == OperationKind::SetAdapterPower && result.reasonCode == QLatin1String("radio-not-authorized"))
        return tr("Bluetooth radio control is not allowed in this session.");
    if (result.kind == OperationKind::SetAdapterPower && result.reasonCode == QLatin1String("radio-stale-target"))
        return tr("The Bluetooth adapter changed. Refresh its current state before trying again.");
    if (result.kind == OperationKind::SetAdapterPower && result.reasonCode == QLatin1String("radio-busy"))
        return tr("Bluetooth is busy. Wait for its current power request to finish.");
    if (result.kind == OperationKind::SetAdapterPower && result.reasonCode == QLatin1String("radio-change-uncertain"))
        return tr("Bluetooth radio recovery could not be confirmed. Check the current power state before retrying.");
    if (result.kind == OperationKind::SetAdapterPower && result.reasonCode == QLatin1String("bluez-power-uncertain"))
        return tr("The Bluetooth controller did not confirm the power change. Check the current power state before retrying.");
  switch (result.status) {
  case OperationStatus::Rejected:
    return tr("That Bluetooth change is unavailable. Check the device and try again.");
  case OperationStatus::Unsupported:
    return tr("That Bluetooth operation is not supported.");
  case OperationStatus::Busy:
    return tr("Bluetooth is busy; wait for the current operation to finish.");
  case OperationStatus::Failed:
    return tr("The Bluetooth change failed. Make sure the device is nearby and try again.");
  case OperationStatus::Uncertain:
    return tr("The Bluetooth operation result is uncertain.");
  case OperationStatus::Succeeded:
    return {};
  }
  return tr("The Bluetooth result could not be understood.");
}

}
