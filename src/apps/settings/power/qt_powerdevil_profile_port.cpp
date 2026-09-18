// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qt_powerdevil_profile_port.h"

namespace QindaQt::Apps::SettingsPower {

QtPowerDevilProfilePolicyPort::QtPowerDevilProfilePolicyPort(
    QindaQt::Session::PowerDevilProfile::PowerDevilProfileAdapter &adapter,
    QObject *parent)
    : AutomaticPowerProfilePolicyPort(parent), m_adapter(adapter) {
  connect(&m_adapter,
          &QindaQt::Session::PowerDevilProfile::PowerDevilProfileAdapter::availabilityChanged,
          this, &AutomaticPowerProfilePolicyPort::availabilityChanged);
  connect(&m_adapter,
          &QindaQt::Session::PowerDevilProfile::PowerDevilProfileAdapter::applyingChanged,
          this, &AutomaticPowerProfilePolicyPort::busyChanged);
  connect(&m_adapter,
          &QindaQt::Session::PowerDevilProfile::PowerDevilProfileAdapter::errorChanged,
          this, &AutomaticPowerProfilePolicyPort::errorChanged);
  connect(&m_adapter,
          &QindaQt::Session::PowerDevilProfile::PowerDevilProfileAdapter::preferencesChanged,
          this, &AutomaticPowerProfilePolicyPort::policyChanged);
  connect(&m_adapter,
          &QindaQt::Session::PowerDevilProfile::PowerDevilProfileAdapter::applyFinished,
          this, &AutomaticPowerProfilePolicyPort::applyFinished);
}

QtPowerDevilProfilePolicyPort::~QtPowerDevilProfilePolicyPort() = default;

bool QtPowerDevilProfilePolicyPort::available() const { return m_adapter.available(); }

bool QtPowerDevilProfilePolicyPort::busy() const { return m_adapter.applying(); }

QString QtPowerDevilProfilePolicyPort::errorText() const { return m_adapter.error(); }

QString QtPowerDevilProfilePolicyPort::acProfileId() const {
  return m_adapter.acProfileId();
}

QString QtPowerDevilProfilePolicyPort::batteryProfileId() const {
  return m_adapter.batteryProfileId();
}

QString QtPowerDevilProfilePolicyPort::lowBatteryProfileId() const {
  return m_adapter.lowBatteryProfileId();
}

void QtPowerDevilProfilePolicyPort::applyProfiles(
    const QString &acProfileId, const QString &batteryProfileId,
    const QString &lowBatteryProfileId) {
  m_adapter.apply(acProfileId, batteryProfileId, lowBatteryProfileId);
}

} // namespace QindaQt::Apps::SettingsPower
