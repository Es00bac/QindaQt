// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

namespace QindaQt::Apps::SettingsPower {

// AGENT-CONTRACT: QML consumes only this injected port for lid-close and
// power-button policy (ADR-0132). The concrete implementation forwards to the
// session's PowerDevil adapter; models and QML never import that adapter, its
// KConfig file, or any D-Bus. Values are the PowerDevil::PowerButtonAction
// numbers cited in ADR-0132; wakeWithExternalMonitor is the inverse of the
// adapter's InhibitLidActionWhenExternalMonitorPresent.
class PowerButtonLidPolicyPort : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
  Q_PROPERTY(QString errorText READ errorText NOTIFY errorChanged)
  Q_PROPERTY(quint32 lidAction READ lidAction NOTIFY policyChanged)
  Q_PROPERTY(bool wakeWithExternalMonitor READ wakeWithExternalMonitor
                 NOTIFY policyChanged)
  Q_PROPERTY(quint32 powerButtonAction READ powerButtonAction NOTIFY
                 policyChanged)

public:
  using QObject::QObject;
  ~PowerButtonLidPolicyPort() override = default;

  [[nodiscard]] virtual bool available() const = 0;
  [[nodiscard]] virtual bool busy() const = 0;
  [[nodiscard]] virtual QString errorText() const = 0;
  [[nodiscard]] virtual quint32 lidAction() const = 0;
  [[nodiscard]] virtual bool wakeWithExternalMonitor() const = 0;
  [[nodiscard]] virtual quint32 powerButtonAction() const = 0;

  // One write per user action. The implementation forwards to the owning
  // adapter, which validates both action values and rejects without writing
  // when unavailable or busy; the result arrives through applyFinished.
  Q_INVOKABLE virtual void applyPolicy(quint32 lidAction,
                                       bool wakeWithExternalMonitor,
                                       quint32 powerButtonAction) = 0;

Q_SIGNALS:
  void availabilityChanged();
  void busyChanged();
  void errorChanged();
  void policyChanged();
  void applyFinished(bool success, const QString &error);
};

} // namespace QindaQt::Apps::SettingsPower
