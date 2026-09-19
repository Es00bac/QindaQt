// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace QindaQt::Apps::SettingsPower {

// Process-lifetime QML singleton composition. It owns only the public Power1
// transport/client, the public Display1 transport/client for external-display
// brightness (ADR-0150), and the route models; platform services remain behind
// the public client boundaries.
class PowerRouteComposition final : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(QObject *model READ model CONSTANT)
  Q_PROPERTY(QObject *screenLockSettings READ screenLockSettings CONSTANT)
  Q_PROPERTY(QObject *idleDisplaySettings READ idleDisplaySettings CONSTANT)
  Q_PROPERTY(QObject *screensaverSettings READ screensaverSettings CONSTANT)

public:
  explicit PowerRouteComposition(QObject *parent = nullptr);
  ~PowerRouteComposition() override;

  [[nodiscard]] QObject *model() const;
  [[nodiscard]] QObject *screenLockSettings() const;
  [[nodiscard]] QObject *idleDisplaySettings() const;
  [[nodiscard]] QObject *screensaverSettings() const;

private:
  class Private;
  std::unique_ptr<Private> d;
};

} // namespace QindaQt::Apps::SettingsPower
