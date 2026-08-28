// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "settings_route_registry.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace QindaQt::Apps::SettingsCenter {

// AGENT-CONTRACT: Coordinates active route selection, route availability,
// history, and keyboard traversal for Settings Center. Exposed to QML as the
// navigation authority. Navigation requests to unknown routes are rejected
// without modifying state.
class SettingsNavigationController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString activeRouteId READ activeRouteId WRITE setActiveRouteId
                 NOTIFY activeRouteIdChanged)
  Q_PROPERTY(QString activeRouteComponent READ activeRouteComponent NOTIFY
                 activeRouteChanged)
  Q_PROPERTY(
      QString activeRouteTitle READ activeRouteTitle NOTIFY activeRouteChanged)
  Q_PROPERTY(QString activeRouteDescription READ activeRouteDescription NOTIFY
                 activeRouteChanged)
  Q_PROPERTY(QString activeRouteIconName READ activeRouteIconName NOTIFY
                 activeRouteChanged)
  Q_PROPERTY(bool activeRouteAvailable READ activeRouteAvailable NOTIFY
                 activeRouteChanged)
  Q_PROPERTY(QString activeRouteUnavailableReason READ
                 activeRouteUnavailableReason NOTIFY activeRouteChanged)
  Q_PROPERTY(QString previousRouteId READ previousRouteId NOTIFY
                 previousRouteIdChanged)
  Q_PROPERTY(QVariantList routes READ routesList NOTIFY routesChanged)
  Q_PROPERTY(int routeCount READ routeCount NOTIFY routesChanged)
  Q_PROPERTY(int activeIndex READ activeIndex NOTIFY activeRouteIdChanged)

public:
  explicit SettingsNavigationController(
      const SettingsRouteRegistry &registry,
      const QString &initialRouteId = QStringLiteral("notifications"),
      QObject *parent = nullptr);

  [[nodiscard]] QString activeRouteId() const noexcept {
    return m_activeRouteId;
  }
  [[nodiscard]] QString activeRouteTitle() const;
  [[nodiscard]] QString activeRouteComponent() const;
  [[nodiscard]] QString activeRouteDescription() const;
  [[nodiscard]] QString activeRouteIconName() const;
  [[nodiscard]] bool activeRouteAvailable() const noexcept;
  [[nodiscard]] QString activeRouteUnavailableReason() const;
  [[nodiscard]] QString previousRouteId() const noexcept {
    return m_previousRouteId;
  }
  [[nodiscard]] QVariantList routesList() const;
  [[nodiscard]] int routeCount() const noexcept;
  [[nodiscard]] int activeIndex() const noexcept;

  // AGENT-CONTRACT: Invocables for QML interaction and keyboard traversal.
  Q_INVOKABLE bool selectRoute(const QString &routeId);
  Q_INVOKABLE bool selectIndex(int index);
  Q_INVOKABLE bool selectNext();
  Q_INVOKABLE bool selectPrevious();
  Q_INVOKABLE bool hasRoute(const QString &routeId) const noexcept;
  Q_INVOKABLE bool isRouteAvailable(const QString &routeId) const noexcept;
  Q_INVOKABLE QVariantMap routeAt(int index) const;

public Q_SLOTS:
  void setActiveRouteId(const QString &routeId);

Q_SIGNALS:
  void activeRouteIdChanged(const QString &routeId);
  void activeRouteChanged();
  void previousRouteIdChanged(const QString &previousRouteId);
  void routesChanged();
  void routeSelectionRejected(const QString &rejectedRouteId,
                              const QString &reason);

private:
  SettingsRouteRegistry m_registry;
  QString m_activeRouteId;
  QString m_previousRouteId;
};

} // namespace QindaQt::Apps::SettingsCenter
