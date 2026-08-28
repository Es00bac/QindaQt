// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_protocol/network_types.h>

#include <QtCore/QString>

namespace QindaQt::Network {

struct ValidationResult {
  bool accepted = false;
  QString reasonCode;

  friend bool operator==(const ValidationResult &,
                         const ValidationResult &) = default;
};

// These functions borrow values for one call, allocate no retained state, and
// are reentrant. Validation never repairs wire values; adapters normalize
// upstream identity (SSID/BSSID/interface) before assembling an immutable
// candidate, so any value that fails here is rejected whole.
[[nodiscard]] bool isBoundedText(const QString &value,
                                 qsizetype maximumUtf8Bytes);
[[nodiscard]] bool isValidUniqueOwner(const QString &owner);
[[nodiscard]] bool isValidKnownNetworkId(const QString &id);
[[nodiscard]] ValidationResult validateRadio(const Radio &radio);
[[nodiscard]] ValidationResult validateDevice(const Device &device);
[[nodiscard]] ValidationResult validateAccessPoint(const AccessPoint &point,
                                                   const Snapshot &whole);
[[nodiscard]] ValidationResult validateKnownNetwork(const KnownNetwork &network);
[[nodiscard]] ValidationResult validateActiveConnection(
    const ActiveConnection &connection, const Snapshot &whole);
[[nodiscard]] ValidationResult validateSnapshot(const Snapshot &snapshot);
[[nodiscard]] ValidationResult validateOperationResult(
    const OperationResult &result);

} // namespace QindaQt::Network
