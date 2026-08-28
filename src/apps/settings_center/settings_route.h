// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QVariantMap>

namespace QindaQt::Apps::SettingsCenter {

enum class SettingsRouteComponent {
  Notifications,
  Appearance,
};

[[nodiscard]] QString
settingsRouteComponentKey(SettingsRouteComponent component);

// AGENT-CONTRACT: Bounded value type representing one registered settings
// route. Route identifiers are stable, lowercase ASCII tokens. The component
// enum is deliberately closed: adding a page requires extending both the
// registry and the SettingsRouteHost presentation switch, so an unknown page
// cannot render arbitrary QML.
struct SettingsRoute final {
  static constexpr qsizetype MaximumIdLength = 64;
  static constexpr qsizetype MaximumTitleLength = 128;
  static constexpr qsizetype MaximumDescriptionLength = 256;
  static constexpr qsizetype MaximumIconNameLength = 128;
  static constexpr qsizetype MaximumCategoryLength = 64;
  static constexpr qsizetype MaximumUnavailableReasonLength = 256;

  QString id;
  SettingsRouteComponent component = SettingsRouteComponent::Notifications;
  QString title;
  QString description;
  QString iconName;
  QString category;
  bool available = true;
  QString unavailableReason;

  [[nodiscard]] bool isValid() const noexcept;
  [[nodiscard]] QVariantMap toVariantMap() const;
  [[nodiscard]] bool operator==(const SettingsRoute &other) const noexcept;
  [[nodiscard]] bool operator!=(const SettingsRoute &other) const noexcept {
    return !(*this == other);
  }
};

[[nodiscard]] bool isValidRouteId(const QString &id) noexcept;
[[nodiscard]] bool
isValidRouteComponent(SettingsRouteComponent component) noexcept;

} // namespace QindaQt::Apps::SettingsCenter
