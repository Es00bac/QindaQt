// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace QindaQt::Apps::SettingsPower {

// Process-lifetime QML singleton composition. It owns only the public Power1
// transport/client and the route model; platform services remain behind the
// public client boundary.
class PowerRouteComposition final : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(QObject *model READ model CONSTANT)

public:
  explicit PowerRouteComposition(QObject *parent = nullptr);
  ~PowerRouteComposition() override;

  [[nodiscard]] QObject *model() const;

private:
  class Private;
  std::unique_ptr<Private> d;
};

} // namespace QindaQt::Apps::SettingsPower
