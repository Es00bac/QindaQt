// SPDX-License-Identifier: LGPL-3.0-or-later
#include "layout_edit_mutation_p.h"

#include "applet_edit_mutation_p.h"
#include "panel_edit_mutation_p.h"

#include <type_traits>

namespace QindaQt::ShellCustomization {
namespace {

template<typename>
inline constexpr bool alwaysFalse = false;

} // namespace

std::optional<EditingError> LayoutEditMutation::apply(
    Profiles::LayoutProfile &candidate,
    const EditingCommand &command,
    const AppletPlacementValidator &placementValidator)
{
    return std::visit(
        [&candidate, &placementValidator](const auto &value)
            -> std::optional<EditingError> {
            using Command = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Command, AddPanelCommand>
                          || std::is_same_v<Command, RemovePanelCommand>
                          || std::is_same_v<Command, MovePanelCommand>
                          || std::is_same_v<Command, ConfigurePanelCommand>) {
                return PanelEditMutation::apply(candidate, value, placementValidator);
            } else if constexpr (std::is_same_v<Command, InsertAppletCommand>
                                 || std::is_same_v<Command, MoveAppletCommand>
                                 || std::is_same_v<Command, RemoveAppletCommand>
                                 || std::is_same_v<Command, DuplicateAppletCommand>
                                 || std::is_same_v<Command,
                                                   UpdateAppletSettingsCommand>) {
                return AppletEditMutation::apply(candidate, value, placementValidator);
            } else if constexpr (std::is_same_v<Command, UndoCommand>
                                 || std::is_same_v<Command, RedoCommand>
                                 || std::is_same_v<Command, BeginPreviewCommand>
                                 || std::is_same_v<Command, CommitPreviewCommand>
                                 || std::is_same_v<Command, CancelPreviewCommand>) {
                return EditingError{
                    EditingErrorCode::InvalidCommand,
                    QStringLiteral("history and preview commands are coordinator operations"),
                    {},
                    {},
                };
            } else {
                static_assert(alwaysFalse<Command>, "unhandled editing command");
            }
        },
        command);
}

} // namespace QindaQt::ShellCustomization
