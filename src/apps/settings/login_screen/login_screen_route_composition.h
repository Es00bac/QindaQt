// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace QindaQt::Apps::SettingsLoginScreen {

// Route-local composition root (ADR-0225): owns the production paths, the
// pkexec write client and the polkit probe, and hands QML one model. The
// page never learns where any file or bus lives; tests never construct
// this object (they inject their own seams into the model).
class LoginScreenRouteComposition final : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(QObject *model READ model CONSTANT)

public:
  explicit LoginScreenRouteComposition(QObject *parent = nullptr);
  ~LoginScreenRouteComposition() override;
  [[nodiscard]] QObject *model() const;

private:
  class Private;
  std::unique_ptr<Private> d;
};

} // namespace QindaQt::Apps::SettingsLoginScreen
