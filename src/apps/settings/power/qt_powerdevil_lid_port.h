// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/apps/settings_power/lid_power_button_settings.h>

#include <qindaqt/session/powerdevil_lid/powerdevil_lid_adapter.h>

namespace QindaQt::Apps::SettingsPower {

// Route-private adapter port over the session's PowerDevil lid adapter. Only
// the composition root constructs it; QML receives it as the abstract
// PowerButtonLidPolicyPort surface.
class QtPowerDevilLidPolicyPort final : public PowerButtonLidPolicyPort {
  Q_OBJECT

public:
  explicit QtPowerDevilLidPolicyPort(
      QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter &adapter,
      QObject *parent = nullptr);
  ~QtPowerDevilLidPolicyPort() override;

  [[nodiscard]] bool available() const override;
  [[nodiscard]] bool busy() const override;
  [[nodiscard]] QString errorText() const override;
  [[nodiscard]] quint32 lidAction() const override;
  [[nodiscard]] bool wakeWithExternalMonitor() const override;
  [[nodiscard]] quint32 powerButtonAction() const override;
  void applyPolicy(quint32 lidAction, bool wakeWithExternalMonitor,
                   quint32 powerButtonAction) override;

private:
  QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter &m_adapter;
};

} // namespace QindaQt::Apps::SettingsPower
