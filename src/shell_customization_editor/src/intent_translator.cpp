// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_customization_editor/intent_translator.h"

#include <QVector>

#include <type_traits>
#include <variant>

// The editor domain consumes the transaction engine public vocabulary
// throughout; the sibling namespace is imported file-locally per convention.
using namespace QindaQt::ShellCustomization;

namespace QindaQt::ShellCustomizationEditor {

namespace {

// AGENT-NOTE: applet order is one flat list per panel and the rendered zone
// lives in the applet settings (profile schema v1). A zone-crossing drop is
// therefore a move/copy plus an UpdateAppletSettings; CommitPreview collapses
// both into exactly one durable undo step.
QVariantMap settingsWithZone(const QVariantMap &settings, const QString &zone)
{
    QVariantMap adjusted = settings;
    adjusted.insert(QStringLiteral("zone"), zone);
    return adjusted;
}

QString settingsZone(const QVariantMap &settings)
{
    const QVariant value = settings.value(QStringLiteral("zone"));
    return value.isValid() ? value.toString() : QStringLiteral("start");
}

void appendZoneUpdate(QVector<EditingCommand> &commands,
                      const QString &panelId,
                      const QString &appletId,
                      const QVariantMap &sourceSettings,
                      const DropTarget &target,
                      quint64 expectedRevision)
{
    if (settingsZone(sourceSettings) == target.zone) {
        return;
    }
    UpdateAppletSettingsCommand update;
    update.expectedRevision = expectedRevision;
    update.panelId = panelId;
    update.appletId = appletId;
    update.settings = settingsWithZone(sourceSettings, target.zone);
    commands.append(update);
}

} // namespace

// AGENT-GUARD: the variant member order and the IntentKind enumerator order
// must stay in lockstep; intentKind() relies on the index mapping. The
// static_asserts turn a silent reorder into a build failure.
static_assert(std::is_same_v<std::variant_alternative_t<0, CustomizationIntent>, InsertAppletIntent>);
static_assert(std::is_same_v<std::variant_alternative_t<1, CustomizationIntent>, MoveAppletIntent>);
static_assert(std::is_same_v<std::variant_alternative_t<2, CustomizationIntent>, RemoveAppletIntent>);
static_assert(std::is_same_v<std::variant_alternative_t<3, CustomizationIntent>, DuplicateAppletIntent>);
static_assert(std::is_same_v<std::variant_alternative_t<4, CustomizationIntent>, ConfigurePanelIntent>);
static_assert(std::is_same_v<std::variant_alternative_t<5, CustomizationIntent>, MovePanelIntent>);

IntentKind intentKind(const CustomizationIntent &intent) noexcept
{
    return static_cast<IntentKind>(intent.index());
}

CustomizationIntent paletteInsertIntent(const DragPayload &payload)
{
    return InsertAppletIntent{payload.pluginId};
}

CustomizationIntent instanceMoveIntent(const DragPayload &payload)
{
    return MoveAppletIntent{payload.sourcePanelId, payload.sourceAppletId};
}

CustomizationIntent removeIntent(const QString &panelId, const QString &appletId)
{
    return RemoveAppletIntent{panelId, appletId};
}

CustomizationIntent duplicateIntent(const QString &panelId,
                                    const QString &appletId,
                                    const QString &newAppletId)
{
    return DuplicateAppletIntent{panelId, appletId, newAppletId};
}

CustomizationIntent configureIntent(const QString &panelId,
                                    const PanelConfiguration &configuration)
{
    return ConfigurePanelIntent{panelId, configuration};
}

CustomizationIntent movePanelIntent(const QString &panelId,
                                    const QString &outputId,
                                    Profiles::Edge edge,
                                    Profiles::Alignment alignment,
                                    const std::optional<QString> &beforePanelId)
{
    return MovePanelIntent{panelId, outputId, edge, alignment, beforePanelId};
}

QVector<EditingCommand> translateIntent(const CustomizationIntent &intent,
                                        const DropTarget &target,
                                        const TranslationContext &context)
{
    QVector<EditingCommand> commands;

    switch (intentKind(intent)) {
    case IntentKind::InsertApplet: {
        const auto &insert = std::get<InsertAppletIntent>(intent);
        InsertAppletCommand command;
        command.expectedRevision = context.expectedRevision;
        command.panelId = target.panelId;
        command.instanceId = context.newInstanceAppletId;
        command.pluginId = insert.pluginId;
        command.initialSettings = settingsWithZone(context.sourceSettings, target.zone);
        command.beforeAppletId = target.beforeAppletId;
        commands.append(command);
        break;
    }
    case IntentKind::MoveApplet: {
        const auto &move = std::get<MoveAppletIntent>(intent);
        MoveAppletCommand command;
        command.expectedRevision = context.expectedRevision;
        command.sourcePanelId = move.panelId;
        command.appletId = move.appletId;
        command.targetPanelId = target.panelId;
        command.beforeAppletId = target.beforeAppletId;
        commands.append(command);
        // sourceSettings describe where the dragged instance currently lives,
        // which is the target panel once an in-drag move has executed.
        appendZoneUpdate(commands,
                         target.panelId,
                         move.appletId,
                         context.sourceSettings,
                         target,
                         context.expectedRevision);
        break;
    }
    case IntentKind::RemoveApplet: {
        const auto &remove = std::get<RemoveAppletIntent>(intent);
        RemoveAppletCommand command;
        command.expectedRevision = context.expectedRevision;
        command.panelId = remove.panelId;
        command.appletId = remove.appletId;
        commands.append(command);
        break;
    }
    case IntentKind::DuplicateApplet: {
        const auto &duplicate = std::get<DuplicateAppletIntent>(intent);
        DuplicateAppletCommand command;
        command.expectedRevision = context.expectedRevision;
        command.sourcePanelId = duplicate.panelId;
        command.appletId = duplicate.appletId;
        command.targetPanelId = target.panelId;
        command.newAppletId = duplicate.newAppletId;
        command.beforeAppletId = target.beforeAppletId;
        commands.append(command);
        appendZoneUpdate(commands,
                         target.panelId,
                         duplicate.newAppletId,
                         context.sourceSettings,
                         target,
                         context.expectedRevision);
        break;
    }
    case IntentKind::ConfigurePanel: {
        const auto &configure = std::get<ConfigurePanelIntent>(intent);
        ConfigurePanelCommand command;
        command.expectedRevision = context.expectedRevision;
        command.panelId = configure.panelId;
        // AGENT-GUARD: ConfigurePanelCommand replaces every field at once.
        // Sending a partial tuple would silently reset the other fields; the
        // intent must always carry the complete current configuration.
        command.layer = configure.configuration.layer;
        command.hideMode = configure.configuration.hideMode;
        command.rows = configure.configuration.rows;
        command.thickness = configure.configuration.thickness;
        command.length = configure.configuration.length;
        commands.append(command);
        break;
    }
    case IntentKind::MovePanel: {
        const auto &move = std::get<MovePanelIntent>(intent);
        MovePanelCommand command;
        command.expectedRevision = context.expectedRevision;
        command.panelId = move.panelId;
        command.outputId = move.outputId;
        command.edge = move.edge;
        command.alignment = move.alignment;
        command.beforePanelId = move.beforePanelId;
        commands.append(command);
        break;
    }
    }

    return commands;
}

QVector<EditingCommand> gestureSequence(const CustomizationIntent &intent,
                                        const DropTarget &target,
                                        const TranslationContext &context)
{
    QVector<EditingCommand> commands;
    BeginPreviewCommand begin;
    begin.expectedRevision = context.expectedRevision;
    commands.append(begin);
    commands.append(translateIntent(intent, target, context));
    CommitPreviewCommand commit;
    commit.expectedRevision = context.expectedRevision;
    commands.append(commit);
    return commands;
}

} // namespace QindaQt::ShellCustomizationEditor
