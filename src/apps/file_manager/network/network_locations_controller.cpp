// SPDX-License-Identifier: GPL-3.0-or-later
#include "network_locations_controller.h"

#include "connect_request.h"

#include <utility>

namespace QindaQt::Apps::FileManager {
namespace {

[[nodiscard]] QVariantMap asVariant(const NetworkLocationRecord &record, int index) {
  return {{QStringLiteral("id"), record.id},
          {QStringLiteral("name"), record.name},
          {QStringLiteral("url"), record.url.toString()},
          {QStringLiteral("showInPlaces"), record.showInPlaces},
          {QStringLiteral("mountAtLogin"), record.mountAtLogin},
          {QStringLiteral("index"), index}};
}

[[nodiscard]] ConnectRequest requestFrom(const QVariantMap &values) {
  ConnectRequest request;
  request.scheme = values.value(QStringLiteral("scheme")).toString();
  request.host = values.value(QStringLiteral("host")).toString();
  request.port = values.value(QStringLiteral("port")).toString();
  request.remotePath = values.value(QStringLiteral("remotePath")).toString();
  request.displayName = values.value(QStringLiteral("displayName")).toString();
  request.showInPlaces =
      values.value(QStringLiteral("showInPlaces"), true).toBool();
  request.mountAtLogin =
      values.value(QStringLiteral("mountAtLogin"), false).toBool();
  return request;
}

} // namespace

NetworkLocationsController::NetworkLocationsController(
    std::unique_ptr<NetworkLocationsStore> store, QObject *parent)
    : QObject(parent), m_store(std::move(store)) {
  const NetworkLocationsLoadResult loaded = m_store->load();
  m_locations = loaded.locations;
  // Absent is a clean first run: no inventory yet, and nothing to report.
  if (!loaded.ok() && loaded.error != NetworkLocationsError::Absent) {
    m_storeError = loaded.diagnostic;
    return;
  }
  if (!loaded.migratedFromV1) {
    return;
  }
  // ADR-0199: a v1 inventory is rewritten as v2 once, here, so the next
  // launch reads the current schema. A refused rewrite is reported but is not
  // fatal: the records are already loaded and every surface works.
  const NetworkLocationsWriteResult written = m_store->store(m_locations);
  if (!written.ok()) {
    m_storeError = written.diagnostic;
  }
}

QVariantList NetworkLocationsController::locations() const {
  QVariantList published;
  published.reserve(m_locations.size());
  for (int index = 0; index < m_locations.size(); ++index) {
    published.append(asVariant(m_locations.at(index), index));
  }
  return published;
}

QVariantList NetworkLocationsController::placesLocations() const {
  QVariantList published;
  for (int index = 0; index < m_locations.size(); ++index) {
    if (m_locations.at(index).showInPlaces) {
      published.append(asVariant(m_locations.at(index), index));
    }
  }
  return published;
}

QStringList NetworkLocationsController::schemes() const {
  return connectableSchemes();
}

void NetworkLocationsController::setStoreError(const QString &message) {
  if (m_storeError == message) {
    return;
  }
  m_storeError = message;
  Q_EMIT storeErrorChanged();
}

void NetworkLocationsController::setRequestError(const QString &message) {
  if (m_requestError == message) {
    return;
  }
  m_requestError = message;
  Q_EMIT requestErrorChanged();
}

bool NetworkLocationsController::persist(
    const QVector<NetworkLocationRecord> &candidate) {
  const NetworkLocationsWriteResult written = m_store->store(candidate);
  if (!written.ok()) {
    // AGENT-GUARD: the visible list is not updated on a refused write, so
    // the sidebar never shows a location the next launch would not find.
    setStoreError(written.diagnostic);
    return false;
  }
  m_locations = candidate;
  setStoreError({});
  Q_EMIT locationsChanged();
  return true;
}

QString NetworkLocationsController::saveLocation(const QVariantMap &request) {
  const ConnectRequestResult built = buildNetworkLocation(requestFrom(request));
  if (!built.ok()) {
    setRequestError(built.message);
    return {};
  }
  QVector<NetworkLocationRecord> candidate = m_locations;
  // Re-saving the same canonical address updates that card rather than
  // adding a second one -- the store would refuse the duplicate anyway.
  bool replaced = false;
  for (NetworkLocationRecord &existing : candidate) {
    if (existing.id == built.record.id) {
      existing = built.record;
      replaced = true;
      break;
    }
  }
  if (!replaced) {
    if (candidate.size() >= NetworkLocationsStore::maximumLocations) {
      setRequestError(QStringLiteral("There is room for %1 saved locations.")
                          .arg(NetworkLocationsStore::maximumLocations));
      return {};
    }
    candidate.append(built.record);
  }
  if (!persist(candidate)) {
    return {};
  }
  setRequestError({});
  return built.record.url.toString();
}

void NetworkLocationsController::removeLocation(int index) {
  if (index < 0 || index >= m_locations.size()) {
    return;
  }
  QVector<NetworkLocationRecord> candidate = m_locations;
  candidate.removeAt(index);
  persist(candidate);
}

void NetworkLocationsController::clearStoreError() { setStoreError({}); }

void NetworkLocationsController::clearRequestError() { setRequestError({}); }

} // namespace QindaQt::Apps::FileManager
