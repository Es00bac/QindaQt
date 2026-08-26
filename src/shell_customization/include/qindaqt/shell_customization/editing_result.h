// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_customization/editing_commands.h"

#include <QString>
#include <QtTypes>

namespace QindaQt::ShellCustomization {

enum class EditingErrorCode {
    None,
    RepositoryNotReady,
    ReentrantExecution,
    StaleRevision,
    RevisionExhausted,
    InvalidCommand,
    DuplicatePanelId,
    DuplicateAppletId,
    UnknownPanelId,
    UnknownAppletId,
    UnknownAnchorId,
    InvalidManifest,
    ManifestUnavailable,
    UnsupportedAppletPlacement,
    InvalidProfile,
    InvalidLayout,
    PreviewAlreadyActive,
    PreviewNotActive,
    NothingToUndo,
    NothingToRedo,
    NoChange,
};

struct EditingError final {
    EditingErrorCode code = EditingErrorCode::None;
    QString message;
    QString panelId;
    QString appletId;
};

struct EditingResult final {
    EditingCommandKind kind = EditingCommandKind::AddPanel;
    EditingError error;
    quint64 previousRevision = 0;
    quint64 revision = 0;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return error.code == EditingErrorCode::None;
    }
};

} // namespace QindaQt::ShellCustomization
