// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/brightness_model/brightness_types.h>

namespace QindaQt::Brightness {

enum class CompositionError {
  None,
  InvalidFixture,
  InvalidPowerSnapshot,
  InvalidBrightnessValue,
};

struct CompositionResult {
  ModelSnapshot snapshot;
  CompositionError error = CompositionError::None;
  QString reasonCode;

  [[nodiscard]] bool succeeded() const noexcept {
    return error == CompositionError::None;
  }
};

// AGENT-CONTRACT: Both inputs are complete borrowed generations. The result is
// an owned, enumeration-independent projection. It has no requested-value
// field, so an unconfirmed mutation cannot masquerade as authoritative current
// truth. Canonical owner loss produces a successful snapshot with that owner's
// stale rows removed; invalid input returns no partial projection.
[[nodiscard]] CompositionResult
composeBrightness(const FixtureSnapshot &fixture, const PowerView &power);

} // namespace QindaQt::Brightness
