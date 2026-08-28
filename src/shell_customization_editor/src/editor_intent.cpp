// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_customization_editor/editor_intent.h"

#include <utility>

namespace QindaQt::ShellCustomizationEditor {

bool isValidEditorZone(const QString &zone)
{
    return zone == QLatin1String("start")
        || zone == QLatin1String("center")
        || zone == QLatin1String("end");
}

namespace {

IntentValidation failure(IntentErrorCode code, QString message)
{
    return {code, std::move(message)};
}

IntentValidation invalidPlacementTarget(const DropTarget &target)
{
    if (target.panelId.trimmed().isEmpty()) {
        return failure(IntentErrorCode::EmptyPanelId,
                       QStringLiteral("drop target panel must not be blank"));
    }
    if (!isValidEditorZone(target.zone)) {
        return failure(IntentErrorCode::InvalidZone,
                       QStringLiteral("drop target zone '%1' is not offered by this editor")
                           .arg(target.zone));
    }
    return {};
}

} // namespace

IntentValidation validateIntent(const CustomizationIntent &intent, const DropTarget &target)
{
    switch (intentKind(intent)) {
    case IntentKind::InsertApplet: {
        const auto &insert = std::get<InsertAppletIntent>(intent);
        if (insert.pluginId.trimmed().isEmpty()) {
            return failure(IntentErrorCode::EmptyPluginId,
                           QStringLiteral("palette item has no plugin identity"));
        }
        return invalidPlacementTarget(target);
    }
    case IntentKind::MoveApplet: {
        const auto &move = std::get<MoveAppletIntent>(intent);
        if (move.panelId.trimmed().isEmpty()) {
            return failure(IntentErrorCode::EmptyPanelId,
                           QStringLiteral("source panel must not be blank"));
        }
        if (move.appletId.trimmed().isEmpty()) {
            return failure(IntentErrorCode::EmptyAppletId,
                           QStringLiteral("moved applet must not be blank"));
        }
        if (target.beforeAppletId.has_value() && *target.beforeAppletId == move.appletId) {
            return failure(IntentErrorCode::AnchorSelfReference,
                           QStringLiteral("an applet cannot use itself as its move anchor"));
        }
        return invalidPlacementTarget(target);
    }
    case IntentKind::RemoveApplet: {
        const auto &remove = std::get<RemoveAppletIntent>(intent);
        if (remove.panelId.trimmed().isEmpty()) {
            return failure(IntentErrorCode::EmptyPanelId,
                           QStringLiteral("source panel must not be blank"));
        }
        if (remove.appletId.trimmed().isEmpty()) {
            return failure(IntentErrorCode::EmptyAppletId,
                           QStringLiteral("removed applet must not be blank"));
        }
        return {};
    }
    case IntentKind::DuplicateApplet: {
        const auto &duplicate = std::get<DuplicateAppletIntent>(intent);
        if (duplicate.panelId.trimmed().isEmpty()) {
            return failure(IntentErrorCode::EmptyPanelId,
                           QStringLiteral("source panel must not be blank"));
        }
        if (duplicate.appletId.trimmed().isEmpty()) {
            return failure(IntentErrorCode::EmptyAppletId,
                           QStringLiteral("duplicated applet must not be blank"));
        }
        if (duplicate.newAppletId.trimmed().isEmpty()) {
            return failure(IntentErrorCode::EmptyNewAppletId,
                           QStringLiteral("duplicate needs a new applet instance identity"));
        }
        return invalidPlacementTarget(target);
    }
    case IntentKind::ConfigurePanel: {
        const auto &configure = std::get<ConfigurePanelIntent>(intent);
        if (configure.panelId.trimmed().isEmpty()) {
            return failure(IntentErrorCode::EmptyPanelId,
                           QStringLiteral("configured panel must not be blank"));
        }
        const PanelConfiguration &configuration = configure.configuration;
        if (configuration.rows < 1 || configuration.thickness < 1
            || configuration.length < 0.0 || configuration.length > 1.0) {
            return failure(IntentErrorCode::InvalidConfiguration,
                           QStringLiteral("panel configuration is outside the profile bounds"));
        }
        return {};
    }
    case IntentKind::MovePanel: {
        const auto &move = std::get<MovePanelIntent>(intent);
        if (move.panelId.trimmed().isEmpty()) {
            return failure(IntentErrorCode::EmptyPanelId,
                           QStringLiteral("moved panel must not be blank"));
        }
        if (move.outputId.trimmed().isEmpty()) {
            return failure(IntentErrorCode::EmptyPanelId,
                           QStringLiteral("moved panel needs an output selector"));
        }
        return {};
    }
    }
    return {};
}

} // namespace QindaQt::ShellCustomizationEditor
