// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QtGlobal>

namespace QindaQt::Brightness {

enum class MathError {
  None,
  InvalidRange,
  ValueOutOfRange,
};

struct NormalizedResult {
  quint32 value = 0;
  MathError error = MathError::None;

  [[nodiscard]] bool succeeded() const noexcept {
    return error == MathError::None;
  }
};

struct RawResult {
  quint32 value = 0;
  MathError error = MathError::None;

  [[nodiscard]] bool succeeded() const noexcept {
    return error == MathError::None;
  }
};

// AGENT-CONTRACT: Returned values use Power1's 0..10000 integer normalized
// scale. The inputs are borrowed values; results own their integer. These
// functions are pure, reentrant, and use rounded integer arithmetic without
// floating point.
[[nodiscard]] NormalizedResult normalizeRaw(quint32 minimum, quint32 maximum,
                                            quint32 value);
[[nodiscard]] RawResult denormalizeRaw(quint32 minimum, quint32 maximum,
                                       quint32 normalized);

} // namespace QindaQt::Brightness
