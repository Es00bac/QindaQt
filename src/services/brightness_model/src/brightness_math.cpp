// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/brightness_model/brightness_math.h>

#include <qindaqt/services/power_protocol/power_limits.h>

namespace QindaQt::Brightness {

NormalizedResult normalizeRaw(const quint32 minimum, const quint32 maximum,
                              const quint32 value) {
  if (minimum >= maximum || maximum > Power::kMaximumRawBrightness) {
    return {.value = 0, .error = MathError::InvalidRange};
  }
  if (value < minimum || value > maximum) {
    return {.value = 0, .error = MathError::ValueOutOfRange};
  }
  const quint64 range = static_cast<quint64>(maximum) - minimum;
  const quint64 offset = static_cast<quint64>(value) - minimum;
  const quint64 scaled =
      offset * Power::kNormalizedBrightnessMaximum + range / 2U;
  return {.value = static_cast<quint32>(scaled / range),
          .error = MathError::None};
}

RawResult denormalizeRaw(const quint32 minimum, const quint32 maximum,
                         const quint32 normalized) {
  if (minimum >= maximum || maximum > Power::kMaximumRawBrightness) {
    return {.value = 0, .error = MathError::InvalidRange};
  }
  if (normalized > Power::kNormalizedBrightnessMaximum) {
    return {.value = 0, .error = MathError::ValueOutOfRange};
  }
  const quint64 range = static_cast<quint64>(maximum) - minimum;
  const quint64 scaled = static_cast<quint64>(normalized) * range +
                         Power::kNormalizedBrightnessMaximum / 2U;
  return {.value = static_cast<quint32>(
              minimum + scaled / Power::kNormalizedBrightnessMaximum),
          .error = MathError::None};
}

} // namespace QindaQt::Brightness
