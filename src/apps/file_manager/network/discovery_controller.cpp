// SPDX-License-Identifier: GPL-3.0-or-later
#include "discovery_controller.h"

#include <QVariantMap>

#include <algorithm>
#include <utility>

namespace QindaQt::Apps::FileManager {
namespace {

// "qinda-14.local · 100.67.154.111" -- the address is shown so two machines
// advertising the same name are still distinguishable, never to connect with.
[[nodiscard]] QString subtitleFor(const DiscoveredService &service) {
  if (service.address.isEmpty() || service.address == service.host) {
    return service.key;
  }
  return QStringLiteral("%1 · %2").arg(service.key, service.address);
}

[[nodiscard]] QVariantMap asVariant(const DiscoveredService &service, int index) {
  return {{QStringLiteral("key"), service.key},
          {QStringLiteral("name"), service.name},
          {QStringLiteral("host"), service.host},
          {QStringLiteral("scheme"), service.scheme},
          {QStringLiteral("port"), service.port},
          {QStringLiteral("address"), service.address},
          {QStringLiteral("subtitle"), subtitleFor(service)},
          {QStringLiteral("index"), index}};
}

// A stable order, so a second machine appearing does not reshuffle the list
// under the pointer.
[[nodiscard]] bool orderedBefore(const DiscoveredService &left,
                                 const DiscoveredService &right) {
  const int byName = QString::compare(left.name, right.name, Qt::CaseInsensitive);
  return byName != 0 ? byName < 0 : left.key < right.key;
}

} // namespace

DiscoveryController::DiscoveryController(ServiceDiscoveryPtr discovery,
                                         QObject *parent)
    : QObject(parent), m_discovery(std::move(discovery)) {
  if (!m_discovery) {
    return;
  }
  connect(m_discovery.get(), &ServiceDiscovery::serviceFound, this,
          &DiscoveryController::onServiceFound);
  connect(m_discovery.get(), &ServiceDiscovery::serviceLost, this,
          &DiscoveryController::onServiceLost);
  connect(m_discovery.get(), &ServiceDiscovery::unavailable, this,
          &DiscoveryController::onUnavailable);
}

QVariantList DiscoveryController::services() const {
  QVariantList published;
  published.reserve(m_services.size());
  for (int index = 0; index < m_services.size(); ++index) {
    published.append(asVariant(m_services.at(index), index));
  }
  return published;
}

QString DiscoveryController::addressAt(const int index) const {
  return index >= 0 && index < m_services.size() ? m_services.at(index).key
                                                 : QString();
}

void DiscoveryController::setEnabled(const bool enabled) {
  if (!m_discovery || enabled == m_scanning) {
    return;
  }
  m_scanning = enabled;
  Q_EMIT scanningChanged();
  if (enabled) {
    m_discovery->start();
    return;
  }
  // AGENT-GUARD: stopping must empty the list. Leaving stale rows visible
  // would offer servers nothing is watching for any more.
  m_discovery->stop();
  setUnavailableReason({});
  if (!m_services.isEmpty()) {
    m_services.clear();
    Q_EMIT servicesChanged();
  }
}

void DiscoveryController::onServiceFound(const DiscoveredService &service) {
  if (!m_scanning || service.key.isEmpty()) {
    return;
  }
  const auto existing =
      std::find_if(m_services.cbegin(), m_services.cend(),
                   [&service](const DiscoveredService &candidate) {
                     return candidate.key == service.key;
                   });
  if (existing != m_services.cend()) {
    return;
  }
  const auto at = std::lower_bound(m_services.cbegin(), m_services.cend(), service,
                                   orderedBefore);
  m_services.insert(at, service);
  Q_EMIT servicesChanged();
}

void DiscoveryController::onServiceLost(const QString &key) {
  const auto before = m_services.size();
  m_services.removeIf([&key](const DiscoveredService &candidate) {
    return candidate.key == key;
  });
  if (m_services.size() != before) {
    Q_EMIT servicesChanged();
  }
}

void DiscoveryController::onUnavailable(const QString &diagnostic) {
  setUnavailableReason(diagnostic);
}

void DiscoveryController::setUnavailableReason(const QString &message) {
  if (m_unavailableReason == message) {
    return;
  }
  m_unavailableReason = message;
  Q_EMIT unavailableReasonChanged();
}

void DiscoveryController::clearUnavailableReason() { setUnavailableReason({}); }

} // namespace QindaQt::Apps::FileManager
