// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_customization/editing_commands.h"

#include <type_traits>

namespace QindaQt::ShellCustomization {
namespace {

template<typename>
inline constexpr bool alwaysFalse = false;

} // namespace

EditingCommandKind commandKind(const EditingCommand &command) noexcept
{
    return std::visit(
        [](const auto &value) {
            using Command = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Command, AddPanelCommand>) {
                return EditingCommandKind::AddPanel;
            } else if constexpr (std::is_same_v<Command, RemovePanelCommand>) {
                return EditingCommandKind::RemovePanel;
            } else if constexpr (std::is_same_v<Command, MovePanelCommand>) {
                return EditingCommandKind::MovePanel;
            } else if constexpr (std::is_same_v<Command, ConfigurePanelCommand>) {
                return EditingCommandKind::ConfigurePanel;
            } else if constexpr (std::is_same_v<Command, InsertAppletCommand>) {
                return EditingCommandKind::InsertApplet;
            } else if constexpr (std::is_same_v<Command, MoveAppletCommand>) {
                return EditingCommandKind::MoveApplet;
            } else if constexpr (std::is_same_v<Command, RemoveAppletCommand>) {
                return EditingCommandKind::RemoveApplet;
            } else if constexpr (std::is_same_v<Command, DuplicateAppletCommand>) {
                return EditingCommandKind::DuplicateApplet;
            } else if constexpr (std::is_same_v<Command, UpdateAppletSettingsCommand>) {
                return EditingCommandKind::UpdateAppletSettings;
            } else if constexpr (std::is_same_v<Command, UndoCommand>) {
                return EditingCommandKind::Undo;
            } else if constexpr (std::is_same_v<Command, RedoCommand>) {
                return EditingCommandKind::Redo;
            } else if constexpr (std::is_same_v<Command, BeginPreviewCommand>) {
                return EditingCommandKind::BeginPreview;
            } else if constexpr (std::is_same_v<Command, CommitPreviewCommand>) {
                return EditingCommandKind::CommitPreview;
            } else if constexpr (std::is_same_v<Command, CancelPreviewCommand>) {
                return EditingCommandKind::CancelPreview;
            } else {
                static_assert(alwaysFalse<Command>, "unhandled editing command");
            }
        },
        command);
}

quint64 expectedRevision(const EditingCommand &command) noexcept
{
    return std::visit([](const auto &value) { return value.expectedRevision; }, command);
}

} // namespace QindaQt::ShellCustomization
