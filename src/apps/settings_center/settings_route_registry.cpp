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
  const auto registerBuiltIn = [this](const SettingsRoute &route) {
    const bool registered = registerRoute(route);
    Q_ASSERT(registered);
    Q_UNUSED(registered);
  };

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
  registerBuiltIn(notificationsRoute);

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
  registerBuiltIn(appearanceRoute);

  const SettingsRoute displayRoute{
      .id = QStringLiteral("display"),
      .component = SettingsRouteComponent::Display,
      .title = QCoreApplication::translate("SettingsCenter", "Display"),
      .description = QCoreApplication::translate(
          "SettingsCenter", "Monitors, resolution, scaling, and orientation"),
      .iconName = QStringLiteral("preferences-desktop-display"),
      .category = QCoreApplication::translate("SettingsCenter", "Hardware"),
      .available = true,
      .unavailableReason = QString(),
  };
  registerBuiltIn(displayRoute);

  const SettingsRoute networkRoute{
      .id = QStringLiteral("network"),
      .component = SettingsRouteComponent::Network,
      .title = QCoreApplication::translate("SettingsCenter", "Network"),
      .description = QCoreApplication::translate(
          "SettingsCenter", "Connectivity, devices, and saved networks"),
      .iconName = QStringLiteral("preferences-system-network"),
      .category = QCoreApplication::translate("SettingsCenter", "Hardware"),
      .available = true,
      .unavailableReason = QString(),
  };
  registerBuiltIn(networkRoute);

  const SettingsRoute customizeRoute{
      .id = QStringLiteral("customize"),
      .component = SettingsRouteComponent::Customize,
      .title = QCoreApplication::translate("SettingsCenter", "Customize"),
      .description = QCoreApplication::translate(
          "SettingsCenter", "Panels, applets, placement, and layout profiles"),
      .iconName = QStringLiteral("preferences-desktop-plasma"),
      .category =
          QCoreApplication::translate("SettingsCenter", "Personalization"),
      .available = true,
      .unavailableReason = QString(),
  };
  registerBuiltIn(customizeRoute);

  const SettingsRoute audioRoute{
      .id = QStringLiteral("audio"),
      .component = SettingsRouteComponent::Audio,
      .title = QCoreApplication::translate("SettingsCenter", "Audio"),
      .description = QCoreApplication::translate(
          "SettingsCenter", "Output, input, volumes, and application streams"),
      .iconName = QStringLiteral("audio-card"),
      .category = QCoreApplication::translate("SettingsCenter", "Hardware"),
      .available = true,
      .unavailableReason = QString(),
  };
  registerBuiltIn(audioRoute);

  const SettingsRoute bluetoothRoute{
      .id = QStringLiteral("bluetooth"),
      .component = SettingsRouteComponent::Bluetooth,
      .title = QCoreApplication::translate("SettingsCenter", "Bluetooth"),
      .description = QCoreApplication::translate(
          "SettingsCenter", "Adapters, discovery, and paired devices"),
      .iconName = QStringLiteral("preferences-system-bluetooth"),
      .category = QCoreApplication::translate("SettingsCenter", "Hardware"),
      .available = true,
      .unavailableReason = QString(),
  };
  registerBuiltIn(bluetoothRoute);

  const SettingsRoute powerRoute{
      .id = QStringLiteral("power"),
      .component = SettingsRouteComponent::Power,
      .title = QCoreApplication::translate("SettingsCenter", "Power"),
      .description = QCoreApplication::translate(
          "SettingsCenter", "Power supplies, profiles, and brightness"),
      .iconName = QStringLiteral("preferences-system-power-management"),
      .category = QCoreApplication::translate("SettingsCenter", "Hardware"),
      .available = true,
      .unavailableReason = QString(),
  };
  registerBuiltIn(powerRoute);

  const SettingsRoute clipboardRoute{
      .id = QStringLiteral("clipboard"),
      .component = SettingsRouteComponent::Clipboard,
      .title = QCoreApplication::translate("SettingsCenter", "Clipboard"),
      .description = QCoreApplication::translate(
          "SettingsCenter", "Private history preference, state, and clearing"),
      .iconName = QStringLiteral("edit-paste"),
      .category = QCoreApplication::translate("SettingsCenter", "Personalization"),
      .available = true,
      .unavailableReason = QString(),
  };
  registerBuiltIn(clipboardRoute);

  const SettingsRoute colorRoute{
      .id = QStringLiteral("color"),
      .component = SettingsRouteComponent::Color,
      .title = QCoreApplication::translate("SettingsCenter", "Color"),
      .description = QCoreApplication::translate(
          "SettingsCenter", "ICC color profiles for each display"),
      .iconName = QStringLiteral("preferences-desktop-color"),
      .category = QCoreApplication::translate("SettingsCenter", "Hardware"),
      .available = true,
      .unavailableReason = QString(),
  };
  registerBuiltIn(colorRoute);

  registerAppendedRoutes();
}

void SettingsRouteRegistry::registerAppendedRoutes() {
  // AGENT-CONTRACT: Routes added after the original ten live here. Each is
  // appended last so every existing route index, shortcut and traversal
  // position stays stable (ADR-0128), and keeping them in their own function
  // is what stops the builder growing past its decomposition limit as the
  // desktop gains routes.
  const auto registerBuiltIn = [this](const SettingsRoute &route) {
    const bool registered = registerRoute(route);
    Q_ASSERT(registered);
    Q_UNUSED(registered);
  };

  // AGENT-GUARD: Appended last so every existing route index, shortcut, and
  // traversal order stays stable (ADR-0128).
  const SettingsRoute accessibilityRoute{
      .id = QStringLiteral("accessibility"),
      .component = SettingsRouteComponent::Accessibility,
      .title = QCoreApplication::translate("SettingsCenter", "Accessibility"),
      .description = QCoreApplication::translate(
          "SettingsCenter", "Contrast, motion, transparency, and text scale"),
      .iconName = QStringLiteral("preferences-desktop-accessibility"),
      .category = QCoreApplication::translate("SettingsCenter", "General"),
      .available = true,
      .unavailableReason = QString(),
  };
  registerBuiltIn(accessibilityRoute);

  // AGENT-GUARD: Appended last so every existing route index, shortcut, and
  // traversal order stays stable (ADR-0128); the Input route continues that
  // rule (ADR-0134).
  const SettingsRoute inputRoute{
      .id = QStringLiteral("input"),
      .component = SettingsRouteComponent::Input,
      .title = QCoreApplication::translate("SettingsCenter", "Input"),
      .description = QCoreApplication::translate(
          "SettingsCenter", "Mouse, touchpad, keyboard, and shortcuts"),
      .iconName = QStringLiteral("preferences-desktop-peripherals"),
      .category = QCoreApplication::translate("SettingsCenter", "Hardware"),
      .available = true,
      .unavailableReason = QString(),
  };
  registerBuiltIn(inputRoute);

  // AGENT-GUARD: Appended last so every existing route index, shortcut, and
  // traversal order stays stable (ADR-0128); the Streaming route continues
  // that rule.
  const SettingsRoute streamingRoute{
      .id = QStringLiteral("streaming"),
      .component = SettingsRouteComponent::Streaming,
      .title = QCoreApplication::translate("SettingsCenter", "Streaming"),
      .description = QCoreApplication::translate(
          "SettingsCenter", "Recording, streaming, and the virtual camera"),
      .iconName = QStringLiteral("camera-video"),
      .category = QCoreApplication::translate("SettingsCenter", "Hardware"),
      .available = true,
      .unavailableReason = QString(),
  };
  registerBuiltIn(streamingRoute);
  registerDateTimeRoute();
  registerWindowsRoute();
  registerDefaultApplicationsRoute();
  registerAboutComputerRoute();
  registerStartupRoute();
  registerLoginScreenRoute();
}

void SettingsRouteRegistry::registerDateTimeRoute() {
  // ADR-0211: the clock and region page. Every control on it acts on a real
  // service -- timedate1 for the zone and automatic time, the Calendar's own
  // Settings1 key for the first day of the week -- so the route is registered
  // unconditionally and reports unavailability from the page itself when the
  // platform service cannot be reached.
  const SettingsRoute dateTimeRoute{
      .id = QStringLiteral("datetime"),
      .component = SettingsRouteComponent::DateTime,
      .title = QCoreApplication::translate("SettingsCenter", "Date & time"),
      .description = QCoreApplication::translate(
          "SettingsCenter", "Time zone, automatic time, and the first day of the week"),
      .iconName = QStringLiteral("preferences-system-time"),
      .category = QCoreApplication::translate("SettingsCenter", "General"),
      .available = true,
      .unavailableReason = QString(),
  };
  const bool registered = registerRoute(dateTimeRoute);
  Q_ASSERT(registered);
  Q_UNUSED(registered);
}

void SettingsRouteRegistry::registerWindowsRoute() {
  // AGENT-GUARD: Appended last so every existing route index, shortcut, and
  // traversal order stays stable (ADR-0128); the Windows & workspaces route
  // continues that rule (ADR-0210).
  const SettingsRoute windowsRoute{
      .id = QStringLiteral("windows"),
      .component = SettingsRouteComponent::Windows,
      .title = QCoreApplication::translate("SettingsCenter", "Windows & workspaces"),
      .description = QCoreApplication::translate(
          "SettingsCenter", "Focus, docking chord, snapping, and window groups"),
      .iconName = QStringLiteral("preferences-system-windows"),
      .category = QCoreApplication::translate("SettingsCenter", "Personalization"),
      .available = true,
      .unavailableReason = QString(),
  };
  const bool registered = registerRoute(windowsRoute);
  Q_ASSERT(registered);
  Q_UNUSED(registered);
}

void SettingsRouteRegistry::registerDefaultApplicationsRoute() {
  // AGENT-GUARD: appended after Windows & workspaces, so every route index,
  // shortcut and traversal position before it is unchanged (ADR-0128).
  const SettingsRoute defaultApplicationsRoute{
      .id = QStringLiteral("default-apps"),
      .component = SettingsRouteComponent::DefaultApplications,
      .title =
          QCoreApplication::translate("SettingsCenter", "Default applications"),
      .description = QCoreApplication::translate(
          "SettingsCenter",
          "Browser, mail, file manager, editor, and media players"),
      .iconName = QStringLiteral("preferences-desktop-default-applications"),
      .category = QCoreApplication::translate("SettingsCenter", "General"),
      .available = true,
      .unavailableReason = QString(),
  };
  const bool registered = registerRoute(defaultApplicationsRoute);
  Q_ASSERT(registered);
  Q_UNUSED(registered);
}

void SettingsRouteRegistry::registerAboutComputerRoute() {
  // Appended after Default applications, same rule (ADR-0128).
  const SettingsRoute aboutComputerRoute{
      .id = QStringLiteral("about-computer"),
      .component = SettingsRouteComponent::AboutComputer,
      .title =
          QCoreApplication::translate("SettingsCenter", "About this computer"),
      .description = QCoreApplication::translate(
          "SettingsCenter",
          "Hostname, hardware, disk, memory, and battery health"),
      .iconName = QStringLiteral("help-about"),
      .category = QCoreApplication::translate("SettingsCenter", "General"),
      .available = true,
      .unavailableReason = QString(),
  };
  const bool registered = registerRoute(aboutComputerRoute);
  Q_ASSERT(registered);
  Q_UNUSED(registered);
}

void SettingsRouteRegistry::registerStartupRoute() {
  // Appended after About this computer, same rule (ADR-0128).
  const SettingsRoute startupRoute{
      .id = QStringLiteral("startup"),
      .component = SettingsRouteComponent::Startup,
      .title = QCoreApplication::translate("SettingsCenter",
                                           "Startup applications"),
      .description = QCoreApplication::translate(
          "SettingsCenter", "Choose what launches when you log in"),
      .iconName = QStringLiteral("system-run"),
      .category = QCoreApplication::translate("SettingsCenter", "General"),
      .available = true,
      .unavailableReason = QString(),
  };
  const bool registered = registerRoute(startupRoute);
  Q_ASSERT(registered);
  Q_UNUSED(registered);
}

void SettingsRouteRegistry::registerLoginScreenRoute() {
  // ADR-0225: appended after Startup applications, so every existing route
  // index, shortcut and traversal position stays unchanged (ADR-0128). The
  // route is registered unconditionally: the page itself reports, in plain
  // text, when the polkit helper is missing or the user may not authorize,
  // the same way the Date & time route owns its own degraded truth
  // (ADR-0211).
  const SettingsRoute loginScreenRoute{
      .id = QStringLiteral("login-screen"),
      .component = SettingsRouteComponent::LoginScreen,
      .title =
          QCoreApplication::translate("SettingsCenter", "Login screen"),
      .description = QCoreApplication::translate(
          "SettingsCenter",
          "Theme, default session, and automatic login for SDDM"),
      .iconName = QStringLiteral("system-lock-screen"),
      .category = QCoreApplication::translate("SettingsCenter", "General"),
      .available = true,
      .unavailableReason = QString(),
  };
  const bool registered = registerRoute(loginScreenRoute);
  Q_ASSERT(registered);
  Q_UNUSED(registered);
}

SettingsRouteRegistry SettingsRouteRegistry::createDefault() {
  SettingsRouteRegistry registry;
  registry.registerBuiltInRoutes();
  return registry;
}

} // namespace QindaQt::Apps::SettingsCenter
