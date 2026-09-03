// SPDX-License-Identifier: LGPL-3.0-or-later

#include "bluetooth_settings_projection.h"

#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <QtCore/QCoreApplication>

namespace QindaQt::Apps::SettingsBluetooth::Projection {
namespace {

QString tr(const char *text) {
  return QCoreApplication::translate("BluetoothSettings", text);
}

} // namespace

QString adapterRowId(const QindaQt::Bluetooth::Handle &handle) {
  return QStringLiteral("adapter-%1-%2").arg(handle.epoch).arg(handle.serial);
}

QString deviceRowId(const QindaQt::Bluetooth::Handle &handle) {
  return QStringLiteral("device-%1-%2").arg(handle.epoch).arg(handle.serial);
}

QString deviceClassLabel(const QindaQt::Bluetooth::DeviceClass value) {
  using QindaQt::Bluetooth::DeviceClass;
  switch (value) {
  case DeviceClass::Computer: return tr("Computer");
  case DeviceClass::Phone: return tr("Phone");
  case DeviceClass::AudioVideo: return tr("Audio or video device");
  case DeviceClass::Headset: return tr("Headset");
  case DeviceClass::Headphones: return tr("Headphones");
  case DeviceClass::Keyboard: return tr("Keyboard");
  case DeviceClass::Mouse: return tr("Mouse");
  case DeviceClass::Tablet: return tr("Tablet");
  case DeviceClass::Printer: return tr("Printer");
  case DeviceClass::GameInput: return tr("Game controller");
  case DeviceClass::Wearable: return tr("Wearable device");
  case DeviceClass::Tag: return tr("Tracking tag");
  case DeviceClass::Unknown: return tr("Bluetooth device");
  }
  return tr("Bluetooth device");
}

QString deviceIconName(const QindaQt::Bluetooth::DeviceClass value) {
  using QindaQt::Bluetooth::DeviceClass;
  switch (value) {
  case DeviceClass::Computer: return QStringLiteral("computer");
  case DeviceClass::Phone: return QStringLiteral("phone");
  case DeviceClass::AudioVideo: return QStringLiteral("audio-card");
  case DeviceClass::Headset: return QStringLiteral("audio-headset");
  case DeviceClass::Headphones: return QStringLiteral("audio-headphones");
  case DeviceClass::Keyboard: return QStringLiteral("input-keyboard");
  case DeviceClass::Mouse: return QStringLiteral("input-mouse");
  case DeviceClass::Tablet: return QStringLiteral("input-tablet");
  case DeviceClass::Printer: return QStringLiteral("printer");
  case DeviceClass::GameInput: return QStringLiteral("input-gaming");
  case DeviceClass::Wearable: return QStringLiteral("watch");
  case DeviceClass::Tag: return QStringLiteral("mark-location");
  case DeviceClass::Unknown: return QStringLiteral("bluetooth");
  }
  return QStringLiteral("bluetooth");
}

QString deviceIconText(const QindaQt::Bluetooth::DeviceClass value) {
  using QindaQt::Bluetooth::DeviceClass;
  switch (value) {
  case DeviceClass::Computer: return QStringLiteral("PC");
  case DeviceClass::Phone: return QStringLiteral("PH");
  case DeviceClass::AudioVideo: return QStringLiteral("AV");
  case DeviceClass::Headset: return QStringLiteral("HS");
  case DeviceClass::Headphones: return QStringLiteral("HP");
  case DeviceClass::Keyboard: return QStringLiteral("KB");
  case DeviceClass::Mouse: return QStringLiteral("MS");
  case DeviceClass::Tablet: return QStringLiteral("TB");
  case DeviceClass::Printer: return QStringLiteral("PR");
  case DeviceClass::GameInput: return QStringLiteral("GP");
  case DeviceClass::Wearable: return QStringLiteral("WR");
  case DeviceClass::Tag: return QStringLiteral("TG");
  case DeviceClass::Unknown: return QStringLiteral("BT");
  }
  return QStringLiteral("BT");
}

QString boundedNonAddressName(const QString &name, const QString &fallback) {
  const QString trimmed = name.trimmed();
  // AGENT-GUARD: A platform alias can itself spell a hardware address. The
  // route does not need addresses, so keep them out of visible and accessible
  // copies even though Bluetooth1 legitimately retains them.
  return trimmed.isEmpty() || QindaQt::Bluetooth::isCanonicalAddress(trimmed)
      ? fallback : trimmed;
}

} // namespace QindaQt::Apps::SettingsBluetooth::Projection
