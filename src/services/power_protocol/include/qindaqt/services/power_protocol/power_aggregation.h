// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_protocol/power_types.h>

namespace QindaQt::Power {

enum class AggregationError {
  None,
  TooManySupplies,
  InvalidSupply,
  MixedEpoch,
  DuplicateHandle,
  ArithmeticOverflow,
};

struct AggregationResult {
  CompositeBattery composite;
  AggregationError error = AggregationError::None;
  QString reasonCode;

  [[nodiscard]] bool succeeded() const noexcept {
    return error == AggregationError::None;
  }
};

// AGENT-CONTRACT: This pure function borrows a complete candidate list for one
// call and returns an owned aggregate. It is order-independent, reentrant, and
// never estimates time-to-empty/full. Invalid input returns a canonical empty
// aggregate and typed error; callers keep their prior published snapshot.
[[nodiscard]] AggregationResult
aggregatePowerSupplies(const QList<PowerSupply> &supplies);

} // namespace QindaQt::Power
