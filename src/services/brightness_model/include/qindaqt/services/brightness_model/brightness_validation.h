// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/brightness_model/brightness_types.h>

namespace QindaQt::Brightness {

enum class FixtureError {
  None,
  InvalidLineage,
  TooManyDisplays,
  InvalidText,
  DuplicateStableId,
  DuplicateBacklightMapping,
  InvalidReplication,
};

struct FixtureValidationResult {
  FixtureError error = FixtureError::None;
  QString reasonCode;

  [[nodiscard]] bool accepted() const noexcept {
    return error == FixtureError::None;
  }
};

// AGENT-CONTRACT: Validation is total and does not mutate the borrowed fixture.
// Owner loss is canonical only when lineage and displays are cleared,
// preventing a caller from accidentally retaining a stale generation.
[[nodiscard]] FixtureValidationResult
validateFixture(const FixtureSnapshot &snapshot);

} // namespace QindaQt::Brightness
