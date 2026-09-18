// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "service_discovery.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVector>

#include <memory>

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: the GUI-thread owner of the "Nearby" list for one window
// (ADR-0197). It owns the injected ServiceDiscovery's lifetime, the visible
// order, and the typed unavailable-reason; it never connects to a server,
// never authenticates, and never navigates. QML opens a nearby server by
// handing its published address to NavigationController, exactly as it does
// for a saved location, so a discovered server that refuses a connection
// lands on the ordinary navigation state pane.
//
// AGENT-GUARD: discovery is advisory and off unless enabled. A null
// ServiceDiscovery is a supported composition (the platform has no Avahi, or
// the user turned discovery off in Preferences): `supported` is then false,
// setEnabled(true) publishes nothing, and every other surface still works.
class DiscoveryController final : public QObject {
  Q_OBJECT

  // Sorted by name then address: {key, name, host, scheme, port, address,
  // subtitle}.
  Q_PROPERTY(QVariantList services READ services NOTIFY servicesChanged FINAL)
  // True while a browse is running.
  Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged FINAL)
  // False when no discovery backend was injected at all; the hub then hides
  // the Nearby section instead of showing an empty one that looks broken.
  Q_PROPERTY(bool supported READ supported CONSTANT FINAL)
  Q_PROPERTY(QString unavailableReason READ unavailableReason NOTIFY unavailableReasonChanged FINAL)

public:
  explicit DiscoveryController(ServiceDiscoveryPtr discovery,
                               QObject *parent = nullptr);

  // Starts or stops browsing. Preferences drives this at startup and whenever
  // the user changes the discovery knob; the hub does not start it by being
  // shown, because browsing is a network activity the user opted into.
  Q_INVOKABLE void setEnabled(bool enabled);
  // The canonical address of the service at index, or an empty string.
  // QML hands this straight to NavigationController::navigateTo().
  [[nodiscard]] Q_INVOKABLE QString addressAt(int index) const;
  Q_INVOKABLE void clearUnavailableReason();

  [[nodiscard]] QVariantList services() const;
  [[nodiscard]] bool scanning() const { return m_scanning; }
  [[nodiscard]] bool supported() const { return m_discovery != nullptr; }
  [[nodiscard]] QString unavailableReason() const { return m_unavailableReason; }

  // Test seam independent of QML's QVariantList marshalling.
  [[nodiscard]] QVector<DiscoveredService> serviceValues() const { return m_services; }

signals:
  void servicesChanged();
  void scanningChanged();
  void unavailableReasonChanged();

private:
  void onServiceFound(const DiscoveredService &service);
  void onServiceLost(const QString &key);
  void onUnavailable(const QString &diagnostic);
  void setUnavailableReason(const QString &message);

  ServiceDiscoveryPtr m_discovery;
  QVector<DiscoveredService> m_services;
  bool m_scanning = false;
  QString m_unavailableReason;
};

} // namespace QindaQt::Apps::FileManager
