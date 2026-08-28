// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings_navigation_controller.h"

namespace QindaQt::Apps::SettingsCenter {

SettingsNavigationController::SettingsNavigationController(
    const SettingsRouteRegistry &registry, const QString &initialRouteId,
    QObject *parent)
    : QObject(parent), m_registry(registry) {
  // AGENT-CONTRACT: Initialize with the requested route if valid, or the first
  // available route.
  if (m_registry.hasRoute(initialRouteId)) {
    m_activeRouteId = initialRouteId;
  } else if (!m_registry.isEmpty()) {
    m_activeRouteId = m_registry.routes().constFirst().id;
  } else {
    m_activeRouteId = initialRouteId;
  }
}

QString SettingsNavigationController::activeRouteTitle() const {
  const auto r = m_registry.route(m_activeRouteId);
  return r.has_value() ? r->title : QString();
}

QString SettingsNavigationController::activeRouteComponent() const {
  const auto route = m_registry.route(m_activeRouteId);
  return route.has_value() ? settingsRouteComponentKey(route->component)
                           : QString();
}

QString SettingsNavigationController::activeRouteDescription() const {
  const auto r = m_registry.route(m_activeRouteId);
  return r.has_value() ? r->description : QString();
}

QString SettingsNavigationController::activeRouteIconName() const {
  const auto r = m_registry.route(m_activeRouteId);
  return r.has_value() ? r->iconName : QString();
}

bool SettingsNavigationController::activeRouteAvailable() const noexcept {
  return m_registry.isRouteAvailable(m_activeRouteId);
}

QString SettingsNavigationController::activeRouteUnavailableReason() const {
  const auto r = m_registry.route(m_activeRouteId);
  return r.has_value() ? r->unavailableReason : QString();
}

QVariantList SettingsNavigationController::routesList() const {
  QVariantList list;
  list.reserve(m_registry.routes().size());
  for (const auto &route : m_registry.routes()) {
    list.append(route.toVariantMap());
  }
  return list;
}

int SettingsNavigationController::routeCount() const noexcept {
  return static_cast<int>(m_registry.count());
}

int SettingsNavigationController::activeIndex() const noexcept {
  return static_cast<int>(m_registry.indexOf(m_activeRouteId));
}

bool SettingsNavigationController::selectRoute(const QString &routeId) {
  if (routeId == m_activeRouteId) {
    return true;
  }

  if (!m_registry.hasRoute(routeId)) {
    Q_EMIT routeSelectionRejected(
        routeId,
        QStringLiteral("Unknown settings route ID: '%1'").arg(routeId));
    return false;
  }

  m_previousRouteId = m_activeRouteId;
  m_activeRouteId = routeId;

  Q_EMIT activeRouteIdChanged(m_activeRouteId);
  Q_EMIT activeRouteChanged();
  Q_EMIT previousRouteIdChanged(m_previousRouteId);
  return true;
}

void SettingsNavigationController::setActiveRouteId(const QString &routeId) {
  selectRoute(routeId);
}

bool SettingsNavigationController::selectIndex(int index) {
  if (index < 0 || index >= m_registry.count()) {
    return false;
  }
  return selectRoute(m_registry.routes().at(index).id);
}

bool SettingsNavigationController::selectNext() {
  if (m_registry.isEmpty()) {
    return false;
  }
  const qsizetype current = m_registry.indexOf(m_activeRouteId);
  const qsizetype next = (current + 1) % m_registry.count();
  return selectIndex(static_cast<int>(next));
}

bool SettingsNavigationController::selectPrevious() {
  if (m_registry.isEmpty()) {
    return false;
  }
  const qsizetype current = m_registry.indexOf(m_activeRouteId);
  const qsizetype prev =
      (current <= 0) ? (m_registry.count() - 1) : (current - 1);
  return selectIndex(static_cast<int>(prev));
}

bool SettingsNavigationController::hasRoute(
    const QString &routeId) const noexcept {
  return m_registry.hasRoute(routeId);
}

bool SettingsNavigationController::isRouteAvailable(
    const QString &routeId) const noexcept {
  return m_registry.isRouteAvailable(routeId);
}

QVariantMap SettingsNavigationController::routeAt(int index) const {
  if (index < 0 || index >= m_registry.count()) {
    return {};
  }
  return m_registry.routes().at(index).toVariantMap();
}

} // namespace QindaQt::Apps::SettingsCenter
