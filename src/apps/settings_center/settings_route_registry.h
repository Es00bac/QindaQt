// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "settings_route.h"

#include <QList>
#include <optional>

namespace QindaQt::Apps::SettingsCenter {

// AGENT-CONTRACT: Bounded registry of settings routes.
// Enforces uniqueness of route IDs, valid route constraints, and maximum
// capacity (max 64 routes). Preserves insertion order for deterministic
// navigation list and keyboard traversal.
class SettingsRouteRegistry final {
public:
  static constexpr qsizetype MaximumRouteCount = 64;

  SettingsRouteRegistry() = default;

  // AGENT-CONTRACT: Returns true if the route is valid, unique, and registered
  // within bounds.
  [[nodiscard]] bool registerRoute(const SettingsRoute &route,
                                   QString *error = nullptr);

  // AGENT-CONTRACT: Query registered routes.
  [[nodiscard]] bool hasRoute(const QString &id) const noexcept;
  [[nodiscard]] std::optional<SettingsRoute> route(const QString &id) const;
  [[nodiscard]] const QList<SettingsRoute> &routes() const noexcept {
    return m_routes;
  }
  [[nodiscard]] qsizetype count() const noexcept { return m_routes.size(); }
  [[nodiscard]] bool isEmpty() const noexcept { return m_routes.isEmpty(); }
  [[nodiscard]] qsizetype indexOf(const QString &id) const noexcept;
  [[nodiscard]] bool isRouteAvailable(const QString &id) const noexcept;

  [[nodiscard]] static SettingsRouteRegistry createDefault();

private:
  void registerBuiltInRoutes();
  // Routes appended after the original ten (ADR-0128).
  void registerAppendedRoutes();
  // ADR-0211: registered from its own function rather than appended to
  // registerBuiltInRoutes(), which is at its function-length limit. A route
  // added from here on gets its own function for the same reason.
  void registerDateTimeRoute();
  // ADR-0211: appended after every earlier route so indices stay stable.
  void registerWindowsRoute();
  void registerDefaultApplicationsRoute();
  void registerDefaultApplicationsRoute();
  void registerBuiltInRoutesFirstHalf();
  void registerBuiltInRoutesSecondHalf();
  void registerBuiltIn(const SettingsRoute &route);

  QList<SettingsRoute> m_routes;
};

} // namespace QindaQt::Apps::SettingsCenter
