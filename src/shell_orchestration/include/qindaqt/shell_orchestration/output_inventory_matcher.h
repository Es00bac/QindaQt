// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_layout/panel_layout_types.h"

#include <QString>

namespace QindaQt::ShellOrchestration {

enum class OutputInventoryMatchErrorCode {
    None,
    EmptyInventory,
    CountMismatch,
    InvalidOutput,
    DuplicateOutput,
    MissingOutput,
    GeometryMismatch,
    ScaleMismatch,
};

struct OutputInventoryMatchResult {
    OutputInventoryMatchErrorCode code = OutputInventoryMatchErrorCode::None;
    QString outputId;
    QString message;

    [[nodiscard]] bool ok() const noexcept
    {
        return code == OutputInventoryMatchErrorCode::None;
    }
};

class OutputInventoryMatcher final {
public:
    // Proves both inventories describe the same compositor-logical output
    // generation immediately before surface preparation. Identity, count, and
    // logical geometry must agree exactly.
    //
    // The arguments are NOT interchangeable. `compositorInventory` carries the
    // wl_output logical scale, which may be fractional; `qtInventory` carries
    // QScreen::devicePixelRatio(), which Qt's Wayland platform reports as that
    // scale rounded up to an integer buffer scale. The scale check accepts
    // both rulers; see scalesDescribeTheSameOutput() in the implementation
    // before tightening it.
    [[nodiscard]] static OutputInventoryMatchResult match(
        const QVector<ShellLayout::LogicalOutput> &compositorInventory,
        const QVector<ShellLayout::LogicalOutput> &qtInventory);
};

} // namespace QindaQt::ShellOrchestration
