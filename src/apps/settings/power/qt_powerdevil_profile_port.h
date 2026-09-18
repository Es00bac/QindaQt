// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/apps/settings_power/automatic_power_profile_policy_port.h>

#include <qindaqt/session/powerdevil_profile/powerdevil_profile_adapter.h>

namespace QindaQt::Apps::SettingsPower {

// Route-private adapter port over the session's PowerDevil profile adapter.
// Only the composition root constructs it; QML receives it as the abstract
// AutomaticPowerProfilePolicyPort surface.
class QtPowerDevilProfilePolicyPort final : public AutomaticPowerProfilePolicyPort {
  Q_OBJECT

public:
  explicit QtPowerDevilProfilePolicyPort(
      QindaQt::Session::PowerDevilProfile::PowerDevilProfileAdapter &adapter,
      QObject *parent = nullptr);
  ~QtPowerDevilProfilePolicyPort() override;

  [[nodiscard]] bool available() const override;
  [[nodiscard]] bool busy() const override;
  [[nodiscard]] QString errorText() const override;
  [[nodiscard]] QString acProfileId() const override;
  [[nodiscard]] QString batteryProfileId() const override;
  [[nodiscard]] QString lowBatteryProfileId() const override;
  void applyProfiles(const QString &acProfileId, const QString &batteryProfileId,
                     const QString &lowBatteryProfileId) override;

private:
  QindaQt::Session::PowerDevilProfile::PowerDevilProfileAdapter &m_adapter;
};

} // namespace QindaQt::Apps::SettingsPower
