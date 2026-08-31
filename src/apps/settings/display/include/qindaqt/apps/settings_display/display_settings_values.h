// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_protocol/display_types.h>
#include <qindaqt/services/display_topology/topology_types.h>

#include <QtCore/QList>
#include <QtCore/QPoint>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>

namespace QindaQt::Apps::SettingsDisplay {

struct FormattedMode {
  QString id;
  QSize pixelSize;
  quint32 refreshMilliHertz = 0;
  double refreshRateHz = 0.0;
  QString label;
  bool preferred = false;

  [[nodiscard]] QVariantMap toVariantMap() const;
};

struct OutputDraft {
  QString stableId;
  QString connectorName;
  QString label;
  QString manufacturer;
  QString model;
  QSize physicalSizeMillimeters;
  bool internal = false;
  bool enabled = false;
  bool primary = false;
  QString modeId;
  QPoint position;
  QSize logicalSize;
  double scale = 1.0;
  Display::Transform transform = Display::Transform::Normal;
  quint32 priority = 0;
  QString replicationSourceStableId;
  QList<FormattedMode> modes;

  [[nodiscard]] QVariantMap toVariantMap() const;
};

[[nodiscard]] QString formatTransform(Display::Transform transform);
[[nodiscard]] Display::Transform parseTransform(const QString &transformStr);

[[nodiscard]] QString formatModeLabel(const QSize &pixelSize, quint32 refreshMilliHertz);
[[nodiscard]] FormattedMode formatMode(const Display::Mode &mode);
[[nodiscard]] QList<FormattedMode> formatModes(const QList<Display::Mode> &modes);

[[nodiscard]] QString formatTopologyError(DisplayTopology::TopologyError error,
                                          const QString &reasonCode);

} // namespace QindaQt::Apps::SettingsDisplay
