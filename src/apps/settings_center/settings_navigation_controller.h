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
  // Each entry is SettingsRoute::toVariantMap(): identity, presentation,
  // availability, and the ADR-0257 search `keywords` and `destinations`.
  Q_PROPERTY(QVariantList routes READ routesList NOTIFY routesChanged)
  Q_PROPERTY(int routeCount READ routeCount NOTIFY routesChanged)
  Q_PROPERTY(int activeIndex READ activeIndex NOTIFY activeRouteIdChanged)
  // AGENT-CONTRACT: The deep link a launcher passes on the command line
  // (--destination, --select). They are inert data this controller only
  // carries: a route page reads them when it opens and decides whether it
  // recognizes them, so an unknown destination can never leave navigation in
  // a state no page renders.
  Q_PROPERTY(QString requestedDestination READ requestedDestination NOTIFY
                 requestedDeepLinkChanged)
  Q_PROPERTY(QString requestedSelection READ requestedSelection NOTIFY
                 requestedDeepLinkChanged)

public:
  explicit SettingsNavigationController(
      const SettingsRouteRegistry &registry,
      const QString &initialRouteId = QStringLiteral("notifications"),
      QString requestedDestination = {}, QString requestedSelection = {},
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
  [[nodiscard]] QString requestedDestination() const noexcept {
    return m_requestedDestination;
  }
  [[nodiscard]] QString requestedSelection() const noexcept {
    return m_requestedSelection;
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
  // Asks the next page of `routeId` to open a destination with an item
  // selected — the in-process form of the command-line deep link, used when
  // one route sends the user to another (the Display card's "Pen & tablet
  // settings…"). Rejects an unknown route without changing state. Repeating
  // the current request for the active route re-delivers it (the link is
  // cleared and set again), so an open page can react to it (ADR-0257).
  Q_INVOKABLE bool selectRouteDestination(const QString &routeId,
                                          const QString &destination,
                                          const QString &selection = {});

public Q_SLOTS:
  void setActiveRouteId(const QString &routeId);

Q_SIGNALS:
  void activeRouteIdChanged(const QString &routeId);
  void activeRouteChanged();
  void previousRouteIdChanged(const QString &previousRouteId);
  void routesChanged();
  void routeSelectionRejected(const QString &rejectedRouteId,
                              const QString &reason);
  void requestedDeepLinkChanged();

private:
  SettingsRouteRegistry m_registry;
  QString m_activeRouteId;
  QString m_previousRouteId;
  QString m_requestedDestination;
  QString m_requestedSelection;
};

} // namespace QindaQt::Apps::SettingsCenter
