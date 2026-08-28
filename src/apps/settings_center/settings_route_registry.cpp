// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings_route_registry.h"

#include <QCoreApplication>

namespace QindaQt::Apps::SettingsCenter {

bool SettingsRouteRegistry::registerRoute(const SettingsRoute &route,
                                          QString *error) {
  // AGENT-GUARD: Bounded capacity check.
  if (m_routes.size() >= MaximumRouteCount) {
    if (error != nullptr) {
      *error =
          QStringLiteral("Route registry capacity exceeded (maximum %1 routes)")
              .arg(MaximumRouteCount);
    }
    return false;
  }

  // AGENT-GUARD: Validation check.
  if (!route.isValid()) {
    if (error != nullptr) {
      *error = QStringLiteral("Invalid settings route descriptor: '%1'")
                   .arg(route.id);
    }
    return false;
  }

  // AGENT-GUARD: Duplicate ID check.
  if (hasRoute(route.id)) {
    if (error != nullptr) {
      *error =
          QStringLiteral("Duplicate settings route ID: '%1'").arg(route.id);
    }
    return false;
  }

  m_routes.append(route);
  return true;
}

bool SettingsRouteRegistry::hasRoute(const QString &id) const noexcept {
  return indexOf(id) >= 0;
}

std::optional<SettingsRoute>
SettingsRouteRegistry::route(const QString &id) const {
  const qsizetype idx = indexOf(id);
  if (idx < 0) {
    return std::nullopt;
  }
  return m_routes.at(idx);
}

qsizetype SettingsRouteRegistry::indexOf(const QString &id) const noexcept {
  for (qsizetype i = 0; i < m_routes.size(); ++i) {
    if (m_routes.at(i).id == id) {
      return i;
    }
  }
  return -1;
}

bool SettingsRouteRegistry::isRouteAvailable(const QString &id) const noexcept {
  const qsizetype idx = indexOf(id);
  if (idx < 0) {
    return false;
  }
  return m_routes.at(idx).available;
}

void SettingsRouteRegistry::registerBuiltInRoutes() {
  // AGENT-CONTRACT: Standard first-party routes.
  // Preserves deterministic initial navigation order: Notifications, then
  // Appearance.
  const SettingsRoute notificationsRoute{
      .id = QStringLiteral("notifications"),
      .component = SettingsRouteComponent::Notifications,
      .title = QCoreApplication::translate("SettingsCenter", "Notifications"),
      .description = QCoreApplication::translate(
          "SettingsCenter", "Do Not Disturb, alerts, and quieting"),
      .iconName = QStringLiteral("preferences-system-notifications"),
      .category = QCoreApplication::translate("SettingsCenter", "General"),
      .available = true,
      .unavailableReason = QString(),
  };
  const bool notifRegistered = registerRoute(notificationsRoute);
  Q_ASSERT(notifRegistered);
  Q_UNUSED(notifRegistered);

  const SettingsRoute appearanceRoute{
      .id = QStringLiteral("appearance"),
      .component = SettingsRouteComponent::Appearance,
      .title = QCoreApplication::translate("SettingsCenter", "Appearance"),
      .description = QCoreApplication::translate(
          "SettingsCenter",
          "Theme, color scheme, fonts, wallpaper, and scaling"),
      .iconName = QStringLiteral("preferences-desktop-theme"),
      .category =
          QCoreApplication::translate("SettingsCenter", "Personalization"),
      .available = true,
      .unavailableReason = QString(),
  };
  const bool appRegistered = registerRoute(appearanceRoute);
  Q_ASSERT(appRegistered);
  Q_UNUSED(appRegistered);
}

SettingsRouteRegistry SettingsRouteRegistry::createDefault() {
  SettingsRouteRegistry registry;
  registry.registerBuiltInRoutes();
  return registry;
}

} // namespace QindaQt::Apps::SettingsCenter
