// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_customization_editor/editor_intent.h"

#include <cmath>
#include <utility>

namespace QindaQt::ShellCustomizationEditor {

bool isValidEditorZone(const QString &zone)
{
    return zone == QLatin1String("start")
        || zone == QLatin1String("center")
        || zone == QLatin1String("end")
        || zone == QLatin1String("desktop");
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
        const bool enumValuesValid = !Profiles::toString(configuration.layer).isEmpty()
            && !Profiles::toString(configuration.hideMode).isEmpty();
        if (!enumValuesValid || configuration.hideMode == Profiles::HideMode::Always
            || configuration.rows < 1 || configuration.rows > 4
            || configuration.thickness < 20 || configuration.thickness > 192
            || !std::isfinite(configuration.length) || configuration.length < 0.1
            || configuration.length > 1.0) {
            return failure(IntentErrorCode::InvalidConfiguration,
                           QStringLiteral("panel configuration is outside the profile-v1 bounds "
                                          "or requires the deferred reveal slice"));
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
        if (Profiles::toString(move.edge).isEmpty()
            || Profiles::toString(move.alignment).isEmpty()) {
            return failure(IntentErrorCode::InvalidConfiguration,
                           QStringLiteral("panel placement uses an unknown edge or alignment"));
        }
        return {};
    }
    case IntentKind::ConfigureAppletSettings: {
        const auto &configure = std::get<ConfigureAppletSettingsIntent>(intent);
        if (configure.panelId.trimmed().isEmpty()) {
            return failure(IntentErrorCode::EmptyPanelId,
                           QStringLiteral("configured applet's owning panel must not be blank"));
        }
        if (configure.appletId.trimmed().isEmpty()) {
            return failure(IntentErrorCode::EmptyAppletId,
                           QStringLiteral("configured applet must not be blank"));
        }
        return {};
    }
    case IntentKind::AddPanel: {
        const auto &add = std::get<AddPanelIntent>(intent);
        if (add.panel.id.trimmed().isEmpty()) {
            return failure(IntentErrorCode::EmptyPanelId,
                           QStringLiteral("added panel must have an id"));
        }
        if (add.beforePanelId.has_value() && *add.beforePanelId == add.panel.id) {
            return failure(IntentErrorCode::AnchorSelfReference,
                           QStringLiteral("a panel cannot be inserted before itself"));
        }
        // Structural bounds only; the engine owns schema and output policy.
        if (add.panel.rows < 1 || add.panel.thickness < 1
            || !std::isfinite(add.panel.length) || add.panel.length <= 0.0
            || add.panel.length > 1.0) {
            return failure(IntentErrorCode::InvalidConfiguration,
                           QStringLiteral("added panel geometry is out of range"));
        }
        return {};
    }
    case IntentKind::RemovePanel: {
        const auto &remove = std::get<RemovePanelIntent>(intent);
        if (remove.panelId.trimmed().isEmpty()) {
            return failure(IntentErrorCode::EmptyPanelId,
                           QStringLiteral("removed panel must not be blank"));
        }
        return {};
    }
    }
    return {};
}

} // namespace QindaQt::ShellCustomizationEditor
