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
    // Exact equality is intentional: both inventories describe the same
    // compositor-logical generation immediately before surface preparation.
    [[nodiscard]] static OutputInventoryMatchResult match(
        const QVector<ShellLayout::LogicalOutput> &expected,
        const QVector<ShellLayout::LogicalOutput> &observed);
};

} // namespace QindaQt::ShellOrchestration
