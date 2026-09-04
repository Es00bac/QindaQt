// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace QindaQt::Apps::SettingsColor {

// Process-lifetime QML singleton composition. It owns only the public
// Display1 transport/client, the public Settings1 transport/client scoped to
// the assignment key, the public C1 assignment store, the C1 discovery/import
// provider over production roots, and the route model. Compositor and
// system color-daemon authority never cross into this process.
//
// AGENT-CONTRACT: Discovery roots are resolved here (the composition root),
// never inside the C1 provider (ADR-0066). They derive from
// QStandardPaths::GenericDataLocation, so sandboxed tests that redirect
// XDG_DATA_HOME/XDG_DATA_DIRS stay host-independent: System roots are the
// non-writable standard data locations' "color/icc" (production:
// /usr/share/color/icc and /usr/local/share/color/icc), and the single
// UserImported root is the writable data location's "color/icc".
class ColorRouteComposition final : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(QObject *model READ model CONSTANT)

public:
  explicit ColorRouteComposition(QObject *parent = nullptr);
  ~ColorRouteComposition() override;

  [[nodiscard]] QObject *model() const;

private:
  class Private;
  std::unique_ptr<Private> d;
};

} // namespace QindaQt::Apps::SettingsColor
