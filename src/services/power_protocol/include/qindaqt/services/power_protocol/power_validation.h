// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_protocol/power_types.h>

namespace QindaQt::Power {

struct ValidationResult {
  bool accepted = false;
  QString reasonCode;

  friend bool operator==(const ValidationResult &,
                         const ValidationResult &) = default;
};

// These functions borrow values for one call, allocate no retained state, and
// are reentrant. Validation never repairs wire values; adapters sanitize
// upstream text before assembling an immutable candidate.
[[nodiscard]] bool isBoundedText(const QString &value,
                                 qsizetype maximumUtf8Bytes);
[[nodiscard]] QString sanitizeText(QString value, qsizetype maximumUtf8Bytes);
[[nodiscard]] ValidationResult validateSupply(const PowerSupply &supply,
                                              quint64 epoch);
[[nodiscard]] ValidationResult validateSnapshot(const Snapshot &snapshot);
[[nodiscard]] ValidationResult
validateOperationResult(const OperationResult &result);

} // namespace QindaQt::Power
