// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace QindaQt::Apps::SettingsCenter {

enum class SettingsRouteComponent {
  Notifications,
  Appearance,
  Display,
  Network,
  Customize,
  Audio,
  Bluetooth,
  Power,
  Clipboard,
  Color,
  Accessibility,
  Input,
  Streaming,
  DateTime,
  Windows,
  DefaultApplications,
  AboutComputer,
  Startup,
  Screensaver,
  LoginScreen,
  Voice,
};

[[nodiscard]] QString
settingsRouteComponentKey(SettingsRouteComponent component);

// Bounds shared by route and destination search keywords (ADR-0257).
inline constexpr qsizetype MaximumSearchKeywordCount = 32;
inline constexpr qsizetype MaximumSearchKeywordLength = 64;

// True when every keyword is nonblank, NUL-free and bounded, and the list
// itself is bounded. An empty list is valid: keywords are optional.
[[nodiscard]] bool isValidSearchKeywords(const QStringList &keywords) noexcept;

// AGENT-CONTRACT: One sub-page that a route page recognizes as a deep-link
// destination (ADR-0257). The Settings shell only lists it in search and
// hands its id to the page through
// SettingsNavigationController::selectRouteDestination(); the page stays the
// authority on whether it opens it, so a destination listed here must match
// an id the page's own `destinations` model accepts (for Input, see
// src/apps/settings/input/qml/InputPage.qml). The id uses route-id syntax.
struct SettingsRouteDestination final {
  static constexpr qsizetype MaximumTitleLength = 128;

  QString id;
  QString title;
  QStringList keywords{};

  [[nodiscard]] bool isValid() const noexcept;
  [[nodiscard]] QVariantMap toVariantMap() const;
  [[nodiscard]] bool
  operator==(const SettingsRouteDestination &other) const noexcept = default;
};

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
  static constexpr qsizetype MaximumDestinationCount = 16;

  QString id;
  SettingsRouteComponent component = SettingsRouteComponent::Notifications;
  QString title;
  QString description;
  QString iconName;
  QString category;
  bool available = true;
  QString unavailableReason;
  // ADR-0257: optional search metadata. Keywords are extra terms the Settings
  // search palette matches besides the title; destinations are the page's
  // own deep-link sub-pages, unique by id. Both default to empty.
  // AGENT-GUARD: keep the `{}` initializers. Every descriptor is written with
  // designated initializers that stop at unavailableReason, and without a
  // default member initializer GCC's -Wmissing-field-initializers fails the
  // strict (-Werror) build at each of them.
  QStringList keywords{};
  QList<SettingsRouteDestination> destinations{};

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
