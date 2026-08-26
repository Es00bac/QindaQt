// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_customization/editing_result.h"
#include "qindaqt/shell_layout/panel_layout_types.h"

#include <QVector>

namespace QindaQt::ShellCustomization {

struct CandidateValidation final {
    Profiles::LayoutProfile profile;
    ShellLayout::PanelLayoutResult layout;
    EditingError error;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return error.code == EditingErrorCode::None;
    }
};

class LayoutCandidateValidator final {
public:
    [[nodiscard]] static CandidateValidation validate(
        const Profiles::LayoutProfile &candidate,
        const QVector<ShellLayout::LogicalOutput> &outputs);
};

} // namespace QindaQt::ShellCustomization
