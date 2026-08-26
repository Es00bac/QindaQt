// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "chrometypes.h"

#include <optional>

namespace QindaQt::HybridChrome {

class ChromeLayoutEngine final
{
public:
    // Pure value transformation. The returned plan owns all strings/geometry,
    // is reentrant, and has no affinity to the compositor or GUI thread.
    [[nodiscard]] static std::optional<ChromeRenderPlan> build(
        const ChromeLayoutRequest &request,
        QString *error = nullptr);
};

} // namespace QindaQt::HybridChrome
