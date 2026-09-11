// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qt_powerdevil_lid_port.h"

namespace QindaQt::Apps::SettingsPower {

QtPowerDevilLidPolicyPort::QtPowerDevilLidPolicyPort(
    QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter &adapter,
    QObject *parent)
    : PowerButtonLidPolicyPort(parent), m_adapter(adapter) {
  connect(&m_adapter,
          &QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter::availabilityChanged,
          this, &PowerButtonLidPolicyPort::availabilityChanged);
  connect(&m_adapter,
          &QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter::applyingChanged,
          this, &PowerButtonLidPolicyPort::busyChanged);
  connect(&m_adapter,
          &QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter::errorChanged,
          this, &PowerButtonLidPolicyPort::errorChanged);
  connect(&m_adapter,
          &QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter::preferencesChanged,
          this, &PowerButtonLidPolicyPort::policyChanged);
  connect(&m_adapter,
          &QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter::applyFinished,
          this, &PowerButtonLidPolicyPort::applyFinished);
}

QtPowerDevilLidPolicyPort::~QtPowerDevilLidPolicyPort() = default;

bool QtPowerDevilLidPolicyPort::available() const { return m_adapter.available(); }

bool QtPowerDevilLidPolicyPort::busy() const { return m_adapter.applying(); }

QString QtPowerDevilLidPolicyPort::errorText() const { return m_adapter.error(); }

quint32 QtPowerDevilLidPolicyPort::lidAction() const { return m_adapter.lidAction(); }

bool QtPowerDevilLidPolicyPort::wakeWithExternalMonitor() const {
  return !m_adapter.inhibitLidActionWhenExternalMonitorPresent();
}

quint32 QtPowerDevilLidPolicyPort::powerButtonAction() const {
  return m_adapter.powerButtonAction();
}

void QtPowerDevilLidPolicyPort::applyPolicy(const quint32 lidAction,
                                            const bool wakeWithExternalMonitor,
                                            const quint32 powerButtonAction) {
  m_adapter.apply(lidAction, !wakeWithExternalMonitor, powerButtonAction);
}

} // namespace QindaQt::Apps::SettingsPower
