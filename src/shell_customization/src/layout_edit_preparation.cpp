// SPDX-License-Identifier: LGPL-3.0-or-later
#include "layout_edit_preparation_p.h"

#include "layout_edit_helpers_p.h"
#include "layout_edit_mutation_p.h"

#include <utility>

namespace QindaQt::ShellCustomization {

CandidateValidation LayoutEditPreparation::prepare(
    const Profiles::LayoutProfile &current,
    const EditingCommand &command,
    const AppletPlacementValidator &placementValidator,
    const QVector<ShellLayout::LogicalOutput> &outputs)
{
    Profiles::LayoutProfile candidate = current;
    if (auto mutationError = LayoutEditMutation::apply(
            candidate, command, placementValidator)) {
        return {.profile = {}, .layout = {}, .error = std::move(*mutationError)};
    }

    CandidateValidation validation =
        LayoutCandidateValidator::validate(candidate, outputs);
    if (!validation.succeeded()) {
        return validation;
    }
    if (LayoutEditHelpers::sameProfile(current, validation.profile)) {
        return {
            .profile = {},
            .layout = {},
            .error = {
                EditingErrorCode::NoChange,
                QStringLiteral("layout editing command made no change"),
                {},
                {},
            },
        };
    }
    return validation;
}

} // namespace QindaQt::ShellCustomization
