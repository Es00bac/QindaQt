// SPDX-License-Identifier: LGPL-3.0-or-later
#include "layout_edit_request_p.h"

#include <limits>
#include <utility>

namespace QindaQt::ShellCustomization {
namespace {

EditingError error(EditingErrorCode code, QString message)
{
    return {code, std::move(message), {}, {}};
}

bool consumesReservedPreviewRevision(EditingCommandKind kind) noexcept
{
    return kind != EditingCommandKind::CommitPreview
        && kind != EditingCommandKind::CancelPreview
        && kind != EditingCommandKind::BeginPreview;
}

} // namespace

std::optional<EditingError> editingRequestError(
    bool ready,
    const EditingError &initializationError,
    quint64 requestedRevision,
    quint64 currentRevision,
    bool previewActive,
    EditingCommandKind kind)
{
    if (!ready) {
        EditingError initialization = initializationError;
        initialization.code = EditingErrorCode::RepositoryNotReady;
        return initialization;
    }
    if (requestedRevision != currentRevision) {
        return error(EditingErrorCode::StaleRevision,
                     QStringLiteral("expected revision %1 but current revision is %2")
                         .arg(requestedRevision)
                         .arg(currentRevision));
    }

    constexpr quint64 maximumRevision = std::numeric_limits<quint64>::max();
    if (currentRevision == maximumRevision
        || (!previewActive
            && kind == EditingCommandKind::BeginPreview
            && currentRevision == maximumRevision - 1)
        || (previewActive
            && consumesReservedPreviewRevision(kind)
            && currentRevision == maximumRevision - 1)) {
        return error(
            EditingErrorCode::RevisionExhausted,
            previewActive
                ? QStringLiteral("the final revision is reserved to commit or cancel the active preview")
                : QStringLiteral("layout editing revision is exhausted"));
    }
    return std::nullopt;
}

} // namespace QindaQt::ShellCustomization
