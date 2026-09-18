// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

namespace QindaQt::Apps::SettingsPower {

// AGENT-CONTRACT: QML consumes only this injected port for the per-power-
// source automatic profile switch (Checkpoint L row 5). The concrete
// implementation forwards to the session's PowerDevil profile adapter;
// models and QML never import that adapter, its KConfig file, or any
// D-Bus. A profile id is whatever the system profile daemon currently
// reports through Power1's ProfileState (typically "power-saver"/
// "balanced"/"performance"); an empty id means no automatic switch is
// configured for that source.
class AutomaticPowerProfilePolicyPort : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
  Q_PROPERTY(QString errorText READ errorText NOTIFY errorChanged)
  Q_PROPERTY(QString acProfileId READ acProfileId NOTIFY policyChanged)
  Q_PROPERTY(QString batteryProfileId READ batteryProfileId NOTIFY policyChanged)
  Q_PROPERTY(QString lowBatteryProfileId READ lowBatteryProfileId NOTIFY
                 policyChanged)

public:
  using QObject::QObject;
  ~AutomaticPowerProfilePolicyPort() override = default;

  [[nodiscard]] virtual bool available() const = 0;
  [[nodiscard]] virtual bool busy() const = 0;
  [[nodiscard]] virtual QString errorText() const = 0;
  [[nodiscard]] virtual QString acProfileId() const = 0;
  [[nodiscard]] virtual QString batteryProfileId() const = 0;
  [[nodiscard]] virtual QString lowBatteryProfileId() const = 0;

  // One write per user action. The implementation forwards to the owning
  // adapter, which rejects without writing when unavailable or busy; the
  // result arrives through applyFinished. An empty id clears that source's
  // automatic switch rather than writing an empty profile name.
  Q_INVOKABLE virtual void applyProfiles(const QString &acProfileId,
                                         const QString &batteryProfileId,
                                         const QString &lowBatteryProfileId) = 0;

Q_SIGNALS:
  void availabilityChanged();
  void busyChanged();
  void errorChanged();
  void policyChanged();
  void applyFinished(bool success, const QString &error);
};

} // namespace QindaQt::Apps::SettingsPower
