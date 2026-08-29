// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_display/display_settings_values.h>

#include <QtCore/QCoreApplication>
#include <algorithm>

namespace QindaQt::Apps::SettingsDisplay {

QVariantMap FormattedMode::toVariantMap() const {
  return {
      {QStringLiteral("id"), id},
      {QStringLiteral("pixelWidth"), pixelSize.width()},
      {QStringLiteral("pixelHeight"), pixelSize.height()},
      {QStringLiteral("refreshRateHz"), refreshRateHz},
      {QStringLiteral("refreshMilliHertz"), static_cast<qulonglong>(refreshMilliHertz)},
      {QStringLiteral("label"), label},
      {QStringLiteral("preferred"), preferred},
  };
}

QVariantMap OutputDraft::toVariantMap() const {
  QVariantList modesList;
  modesList.reserve(modes.size());
  for (const auto &m : modes) {
    modesList.append(m.toVariantMap());
  }

  return {
      {QStringLiteral("stableId"), stableId},
      {QStringLiteral("connectorName"), connectorName},
      {QStringLiteral("label"), label.isEmpty() ? connectorName : label},
      {QStringLiteral("manufacturer"), manufacturer},
      {QStringLiteral("model"), model},
      {QStringLiteral("internal"), internal},
      {QStringLiteral("enabled"), enabled},
      {QStringLiteral("primary"), primary},
      {QStringLiteral("modeId"), modeId},
      {QStringLiteral("positionX"), position.x()},
      {QStringLiteral("positionY"), position.y()},
      {QStringLiteral("logicalWidth"), logicalSize.width()},
      {QStringLiteral("logicalHeight"), logicalSize.height()},
      {QStringLiteral("scale"), scale},
      {QStringLiteral("transform"), formatTransform(transform)},
      {QStringLiteral("priority"), static_cast<qulonglong>(priority)},
      {QStringLiteral("replicationSourceStableId"), replicationSourceStableId},
      {QStringLiteral("modes"), modesList},
  };
}

QString formatTransform(Display::Transform transform) {
  switch (transform) {
  case Display::Transform::Normal:
    return QStringLiteral("normal");
  case Display::Transform::Rotate90:
    return QStringLiteral("90");
  case Display::Transform::Rotate180:
    return QStringLiteral("180");
  case Display::Transform::Rotate270:
    return QStringLiteral("270");
  case Display::Transform::FlipX:
    return QStringLiteral("flipX");
  case Display::Transform::FlipX90:
    return QStringLiteral("flipX90");
  case Display::Transform::FlipX180:
    return QStringLiteral("flipX180");
  case Display::Transform::FlipX270:
    return QStringLiteral("flipX270");
  }
  return QStringLiteral("normal");
}

Display::Transform parseTransform(const QString &transformStr) {
  if (transformStr == QStringLiteral("90") ||
      transformStr == QStringLiteral("rotate90")) {
    return Display::Transform::Rotate90;
  }
  if (transformStr == QStringLiteral("180") ||
      transformStr == QStringLiteral("rotate180")) {
    return Display::Transform::Rotate180;
  }
  if (transformStr == QStringLiteral("270") ||
      transformStr == QStringLiteral("rotate270")) {
    return Display::Transform::Rotate270;
  }
  if (transformStr == QStringLiteral("flipX")) {
    return Display::Transform::FlipX;
  }
  if (transformStr == QStringLiteral("flipX90")) {
    return Display::Transform::FlipX90;
  }
  if (transformStr == QStringLiteral("flipX180")) {
    return Display::Transform::FlipX180;
  }
  if (transformStr == QStringLiteral("flipX270")) {
    return Display::Transform::FlipX270;
  }
  return Display::Transform::Normal;
}

QString formatModeLabel(const QSize &pixelSize, quint32 refreshMilliHertz) {
  const double hz = static_cast<double>(refreshMilliHertz) / 1000.0;
  // Format clean integer Hz or 2 decimal places if non-integral (e.g. 59.94)
  if (refreshMilliHertz % 1000 == 0) {
    return QCoreApplication::translate("DisplaySettings", "%1 × %2 @ %3 Hz")
        .arg(pixelSize.width())
        .arg(pixelSize.height())
        .arg(static_cast<int>(hz));
  }
  return QCoreApplication::translate("DisplaySettings", "%1 × %2 @ %3 Hz")
      .arg(pixelSize.width())
      .arg(pixelSize.height())
      .arg(hz, 0, 'f', 2);
}

FormattedMode formatMode(const Display::Mode &mode) {
  FormattedMode fm;
  fm.id = mode.id;
  fm.pixelSize = mode.pixelSize;
  fm.refreshMilliHertz = mode.refreshMilliHertz;
  fm.refreshRateHz = static_cast<double>(mode.refreshMilliHertz) / 1000.0;
  fm.label = formatModeLabel(mode.pixelSize, mode.refreshMilliHertz);
  fm.preferred = mode.preferred;
  return fm;
}

QList<FormattedMode> formatModes(const QList<Display::Mode> &modes) {
  QList<FormattedMode> result;
  result.reserve(modes.size());
  for (const auto &m : modes) {
    result.append(formatMode(m));
  }
  // Sort modes: highest resolution first, then highest refresh rate, preferred first
  std::stable_sort(result.begin(), result.end(),
                   [](const FormattedMode &a, const FormattedMode &b) {
                     const qint64 areaA = static_cast<qint64>(a.pixelSize.width()) *
                                          a.pixelSize.height();
                     const qint64 areaB = static_cast<qint64>(b.pixelSize.width()) *
                                          b.pixelSize.height();
                     if (areaA != areaB) {
                       return areaA > areaB;
                     }
                     if (a.refreshMilliHertz != b.refreshMilliHertz) {
                       return a.refreshMilliHertz > b.refreshMilliHertz;
                     }
                     if (a.preferred != b.preferred) {
                       return a.preferred;
                     }
                     return a.id < b.id;
                   });
  return result;
}

QString formatTopologyError(DisplayTopology::TopologyError error,
                            const QString &reasonCode) {
  switch (error) {
  case DisplayTopology::TopologyError::None:
    return {};
  case DisplayTopology::TopologyError::InvalidSnapshot:
    return QCoreApplication::translate("DisplaySettings",
                                       "Invalid server display snapshot.");
  case DisplayTopology::TopologyError::InvalidCandidate:
    return QCoreApplication::translate("DisplaySettings",
                                       "Invalid display configuration candidate.");
  case DisplayTopology::TopologyError::StaleLineage:
    return QCoreApplication::translate(
        "DisplaySettings",
        "Display configuration is stale. Server snapshot has changed.");
  case DisplayTopology::TopologyError::OutputSetMismatch:
    return QCoreApplication::translate("DisplaySettings",
                                       "Output set does not match active displays.");
  case DisplayTopology::TopologyError::AllOutputsDisabled:
    return QCoreApplication::translate(
        "DisplaySettings", "At least one display must remain enabled.");
  case DisplayTopology::TopologyError::InvalidPrimary:
    return QCoreApplication::translate(
        "DisplaySettings", "Exactly one enabled display must be designated as primary.");
  case DisplayTopology::TopologyError::InvalidPriority:
    return QCoreApplication::translate("DisplaySettings",
                                       "Invalid display priority assignment.");
  case DisplayTopology::TopologyError::UnknownMode:
    return QCoreApplication::translate(
        "DisplaySettings", "Selected resolution/mode is not supported by this display.");
  case DisplayTopology::TopologyError::InvalidScale:
    return QCoreApplication::translate(
        "DisplaySettings", "Scale factor must be greater than zero.");
  case DisplayTopology::TopologyError::InvalidCoordinate:
    return QCoreApplication::translate("DisplaySettings",
                                       "Invalid display position coordinates.");
  case DisplayTopology::TopologyError::CoordinateOverflow:
    return QCoreApplication::translate("DisplaySettings",
                                       "Display coordinates exceed bounds.");
  case DisplayTopology::TopologyError::Overlap:
    return QCoreApplication::translate("DisplaySettings",
                                       "Enabled displays cannot overlap each other.");
  case DisplayTopology::TopologyError::UnknownMirrorSource:
    return QCoreApplication::translate("DisplaySettings",
                                       "Unknown display mirror source.");
  case DisplayTopology::TopologyError::MirrorSelfReference:
    return QCoreApplication::translate("DisplaySettings",
                                       "Display cannot mirror itself.");
  case DisplayTopology::TopologyError::MirrorCycle:
    return QCoreApplication::translate("DisplaySettings",
                                       "Circular display mirroring detected.");
  }
  if (!reasonCode.isEmpty()) {
    return reasonCode;
  }
  return QCoreApplication::translate("DisplaySettings",
                                     "Invalid display configuration.");
}

} // namespace QindaQt::Apps::SettingsDisplay
