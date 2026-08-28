// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_customization/editing_commands.h"

#include <utility>
#include <variant>

namespace QindaQt::ShellCustomizationEditor::Internal {

inline QindaQt::ShellCustomization::EditingCommand retagCommand(
    QindaQt::ShellCustomization::EditingCommand command,
    quint64 expectedRevision)
{
    std::visit(
        [expectedRevision](auto &typedCommand) {
            typedCommand.expectedRevision = expectedRevision;
        },
        command);
    return command;
}

} // namespace QindaQt::ShellCustomizationEditor::Internal
