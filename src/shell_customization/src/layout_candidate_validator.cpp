// SPDX-License-Identifier: LGPL-3.0-or-later
#include "layout_candidate_validator_p.h"

#include "qindaqt/profiles/profile_loader.h"
#include "qindaqt/profiles/profile_validation.h"
#include "qindaqt/shell_layout/panel_layout_solver.h"

#include <QJsonDocument>

#include <utility>

namespace QindaQt::ShellCustomization {
namespace {

CandidateValidation failure(EditingError error)
{
    CandidateValidation result;
    result.error = std::move(error);
    return result;
}

EditingError profileError(const Profiles::ProfileError &error)
{
    EditingErrorCode code = EditingErrorCode::InvalidProfile;
    if (error.code == Profiles::ProfileErrorCode::DuplicatePanelId) {
        code = EditingErrorCode::DuplicatePanelId;
    } else if (error.code == Profiles::ProfileErrorCode::DuplicateAppletId) {
        code = EditingErrorCode::DuplicateAppletId;
    }
    return {code,
            error.diagnostic(),
            error.panelId,
            error.appletId};
}

} // namespace

CandidateValidation LayoutCandidateValidator::validate(
    const Profiles::LayoutProfile &candidate,
    const QVector<ShellLayout::LogicalOutput> &outputs)
{
    const Profiles::ProfileValidationResult typedValidation =
        Profiles::ProfileValidator::validate(candidate);
    if (!typedValidation.succeeded()) {
        return failure(profileError(typedValidation.error));
    }

    const QByteArray serialized =
        QJsonDocument(candidate.toJson()).toJson(QJsonDocument::Compact);
    const Profiles::LoadResult loaded =
        Profiles::ProfileLoader::fromJson(serialized, QStringLiteral("layout editor candidate"));
    if (!loaded.ok) {
        return failure(profileError(loaded.error));
    }

    ShellLayout::PanelLayoutResult layout =
        ShellLayout::PanelLayoutSolver::solve(loaded.profile.panels, outputs);
    if (!layout.ok()) {
        return failure({EditingErrorCode::InvalidLayout,
                        layout.error.message,
                        layout.error.panelId,
                        {}});
    }

    // AGENT-GUARD: Publish the loader-normalized value, not the mutation's raw
    // QVariant graph. This makes every visible editor snapshot a schema-v1
    // round-trip value and prevents commit-time persistence from changing it.
    return {.profile = loaded.profile, .layout = std::move(layout), .error = {}};
}

} // namespace QindaQt::ShellCustomization
