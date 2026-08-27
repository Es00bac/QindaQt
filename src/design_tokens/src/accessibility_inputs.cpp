// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/design_tokens/accessibility_inputs.h"

#include <algorithm>
#include <cmath>

namespace QindaQt::DesignTokens {

AccessibilityInputs AccessibilityInputs::normalized() const
{
    AccessibilityInputs result = *this;
    result.basePointSize = std::isfinite(basePointSize)
        ? std::clamp(basePointSize, minimumBasePointSize, maximumBasePointSize)
        : defaultBasePointSize;
    result.textScale = std::isfinite(textScale)
        ? std::clamp(textScale, minimumTextScale, maximumTextScale)
        : defaultTextScale;
    return result;
}

} // namespace QindaQt::DesignTokens
