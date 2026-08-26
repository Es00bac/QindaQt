// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_customization/editing_result.h"

#include <optional>

namespace QindaQt::ShellCustomization {

// AGENT-CONTRACT: Execution and side-effect-free evaluation share this exact
// preflight order so an editor never highlights a command execute() rejects.
[[nodiscard]] std::optional<EditingError> editingRequestError(
    bool ready,
    const EditingError &initializationError,
    quint64 requestedRevision,
    quint64 currentRevision,
    bool previewActive,
    EditingCommandKind kind);

} // namespace QindaQt::ShellCustomization
