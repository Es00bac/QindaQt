// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace QindaQt::Apps::SettingsDateTime {

// The Date & time route's composition root (ADR-0200): it builds the
// production systemd clock/locale service and the purpose-scoped week-start
// preference, and hands QML one model. Nothing else in the route knows which
// bus anything lives on.
class DateTimeRouteComposition final : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(QObject *model READ model CONSTANT)

public:
  explicit DateTimeRouteComposition(QObject *parent = nullptr);
  ~DateTimeRouteComposition() override;
  [[nodiscard]] QObject *model() const;

private:
  class Private;
  std::unique_ptr<Private> d;
};

} // namespace QindaQt::Apps::SettingsDateTime
