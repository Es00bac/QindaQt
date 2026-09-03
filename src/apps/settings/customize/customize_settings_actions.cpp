// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_customize/customize_settings_model.h"

#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/shell_customization_editor/accessibility_identity.h"
#include "qindaqt/shell_customization_editor/keyboard_navigation.h"

#include <QMetaType>

namespace QindaQt::Apps::SettingsCustomize {
namespace {

const Profiles::PanelSpec *panelById(const Profiles::LayoutProfile *profile,
                                     const QString &panelId)
{
    if (profile == nullptr) {
        return nullptr;
    }
    for (const auto &panel : profile->panels) {
        if (panel.id == panelId) {
            return &panel;
        }
    }
    return nullptr;
}

const Profiles::AppletSpec *appletById(const Profiles::PanelSpec *panel,
                                       const QString &appletId)
{
    if (panel == nullptr) {
        return nullptr;
    }
    for (const auto &applet : panel->applets) {
        if (applet.id == appletId) {
            return &applet;
        }
    }
    return nullptr;
}

ShellCustomizationEditor::PanelConfiguration panelConfiguration(
    const Profiles::PanelSpec &panel)
{
    return {panel.layer, panel.hideMode, panel.rows, panel.thickness,
            panel.length};
}

} // namespace

ShellCustomizationEditor::DropTarget CustomizeSettingsModel::targetFromStrings(
    const QString &panelId, const QString &zone,
    const QString &beforeAppletId) const
{
    ShellCustomizationEditor::DropTarget target;
    target.panelId = panelId;
    target.zone = zone;
    if (!beforeAppletId.isEmpty()) {
        target.beforeAppletId = beforeAppletId;
    }
    return target;
}

bool CustomizeSettingsModel::settleEditorOutcome(
    const ShellCustomizationEditor::EditorOutcome &outcome,
    const QString &successAnnouncement)
{
    if (!outcome.ok) {
        if (outcome.code
            == ShellCustomizationEditor::EditorErrorCode::EngineUnavailable) {
            m_editorUnavailable = true;
            setState(State::Unavailable, outcome.message);
        } else {
            setState(m_state, outcome.message);
        }
        refreshProjection();
        return false;
    }
    setState(m_state, {});
    if (!successAnnouncement.isEmpty()) {
        m_announcement = successAnnouncement;
        Q_EMIT announcementChanged();
    }
    refreshProjection();
    return true;
}

bool CustomizeSettingsModel::selectProfile(const QString &profileId)
{
    if (!canEdit() || profileId == m_selectedProfileId) {
        return profileId == m_selectedProfileId;
    }
    if (dirty()) {
        setState(m_state,
                 QStringLiteral("Apply or discard the current layout before selecting another profile"));
        return false;
    }
    const Profiles::LayoutProfile *profile = findProfile(profileId);
    if (profile == nullptr || !rebuild(*profile)) {
        return false;
    }
    m_selectionDirty = profileId != m_confirmedProfileId;
    setState(State::Ready);
    refreshProjection();
    return true;
}

void CustomizeSettingsModel::selectPanel(const QString &panelId)
{
    const auto *profile = m_editor ? m_editor->profile() : nullptr;
    if (panelById(profile, panelId) == nullptr) {
        return;
    }
    m_selectedKind = QStringLiteral("panel");
    m_selectedPanelId = panelId;
    m_selectedAppletId.clear();
    Q_EMIT selectionChanged();
}

void CustomizeSettingsModel::selectApplet(const QString &panelId,
                                           const QString &appletId)
{
    const auto *profile = m_editor ? m_editor->profile() : nullptr;
    if (appletById(panelById(profile, panelId), appletId) == nullptr) {
        return;
    }
    m_selectedKind = QStringLiteral("applet");
    m_selectedPanelId = panelId;
    m_selectedAppletId = appletId;
    Q_EMIT selectionChanged();
}

bool CustomizeSettingsModel::startPaletteDrag(const QString &pluginId)
{
    if (!canEdit() || findManifest(pluginId) == nullptr) {
        return false;
    }
    ShellCustomizationEditor::DragPayload payload;
    payload.kind = ShellCustomizationEditor::PayloadKind::PalettePlugin;
    payload.pluginId = pluginId;
    m_draggedName = findManifest(pluginId)->name;
    m_lastDropAccepted = false;
    m_lastDropReason.clear();
    if (!settleEditorOutcome(m_editor->arm(payload))) {
        return false;
    }
    return settleEditorOutcome(m_editor->beginDrag());
}

bool CustomizeSettingsModel::startAppletDrag(const QString &panelId,
                                              const QString &appletId)
{
    if (!canEdit()) {
        return false;
    }
    const auto *profile = m_editor->profile();
    const auto *applet = appletById(panelById(profile, panelId), appletId);
    if (applet == nullptr) {
        return false;
    }
    ShellCustomizationEditor::DragPayload payload;
    payload.kind = ShellCustomizationEditor::PayloadKind::AppletInstance;
    payload.pluginId = applet->plugin;
    payload.sourcePanelId = panelId;
    payload.sourceAppletId = appletId;
    const auto *manifest = findManifest(applet->plugin);
    m_draggedName = manifest ? manifest->name : applet->plugin;
    m_lastDropAccepted = false;
    m_lastDropReason.clear();
    if (!settleEditorOutcome(m_editor->arm(payload))) {
        return false;
    }
    return settleEditorOutcome(m_editor->beginDrag());
}

bool CustomizeSettingsModel::hoverDropTarget(const QString &panelId,
                                              const QString &zone,
                                              const QString &beforeAppletId)
{
    if (!visualDragActive()) {
        return false;
    }
    const auto target = targetFromStrings(panelId, zone, beforeAppletId);
    const auto outcome = m_editor->hover(target);
    const auto acceptance = m_editor->acceptance();
    m_lastDropAccepted = outcome.ok
        && (!acceptance.has_value() || acceptance->accepted);
    m_lastDropReason = acceptance.has_value() ? acceptance->reason
                                               : outcome.message;
    if (const auto *profile = m_editor->profile(); profile != nullptr) {
        const auto announcement = ShellCustomizationEditor::moveAnnouncement(
            *profile, m_draggedName, target, m_lastDropAccepted,
            m_lastDropReason);
        m_announcement = announcement.message;
        Q_EMIT announcementChanged();
    }
    return settleEditorOutcome(outcome);
}

bool CustomizeSettingsModel::commitDrag()
{
    if (!m_editor || !visualDragActive()) {
        return false;
    }
    const bool committed = settleEditorOutcome(
        m_editor->drop(), QStringLiteral("Layout move committed"));
    m_keyboardMoving = false;
    m_lastDropAccepted = false;
    m_lastDropReason.clear();
    return committed;
}

bool CustomizeSettingsModel::cancelDrag()
{
    if (!m_editor) {
        return false;
    }
    const bool cancelled = settleEditorOutcome(
        m_editor->cancel(), QStringLiteral("Layout move cancelled"));
    m_keyboardMoving = false;
    m_lastDropAccepted = false;
    m_lastDropReason.clear();
    return cancelled;
}

bool CustomizeSettingsModel::keyboardInsert(const QString &pluginId,
                                             const QString &panelId,
                                             const QString &zone,
                                             const QString &beforeAppletId)
{
    return startPaletteDrag(pluginId)
        && hoverDropTarget(panelId, zone, beforeAppletId)
        && commitDrag();
}

bool CustomizeSettingsModel::keyboardMoveMode()
{
    if (m_keyboardMoving) {
        return commitDrag();
    }
    if (m_selectedKind != QLatin1String("applet") || !m_editor) {
        return false;
    }
    const auto *panel = panelById(m_editor->profile(), m_selectedPanelId);
    const auto *applet = appletById(panel, m_selectedAppletId);
    if (panel == nullptr || applet == nullptr) {
        return false;
    }
    m_keyboardTarget.panelId = panel->id;
    m_keyboardTarget.zone = applet->settings
                                .value(QStringLiteral("zone"),
                                       QStringLiteral("start"))
                                .toString();
    m_keyboardTarget.beforeAppletId.reset();
    bool found = false;
    for (const auto &candidate : panel->applets) {
        if (found && candidate.settings
                         .value(QStringLiteral("zone"),
                                QStringLiteral("start"))
                         .toString() == m_keyboardTarget.zone) {
            m_keyboardTarget.beforeAppletId = candidate.id;
            break;
        }
        found = candidate.id == applet->id || found;
    }
    m_keyboardMoving = startAppletDrag(panel->id, applet->id);
    return m_keyboardMoving;
}

bool CustomizeSettingsModel::keyboardStep(const QString &direction)
{
    if (!m_keyboardMoving || !m_editor || m_editor->profile() == nullptr) {
        return false;
    }
    const auto &profile = *m_editor->profile();
    std::optional<ShellCustomizationEditor::DropTarget> stepped;
    if (direction == QLatin1String("next")) {
        stepped = ShellCustomizationEditor::nextSlotInPanel(profile,
                                                            m_keyboardTarget);
    } else if (direction == QLatin1String("previous")) {
        stepped = ShellCustomizationEditor::previousSlotInPanel(
            profile, m_keyboardTarget);
    } else if (direction == QLatin1String("next-zone")) {
        stepped = ShellCustomizationEditor::nextZoneInPanel(profile,
                                                            m_keyboardTarget);
    } else if (direction == QLatin1String("previous-zone")) {
        stepped = ShellCustomizationEditor::previousZoneInPanel(
            profile, m_keyboardTarget);
    } else if (direction == QLatin1String("next-panel")) {
        stepped = ShellCustomizationEditor::nextPanelTarget(profile,
                                                            m_keyboardTarget);
    } else if (direction == QLatin1String("previous-panel")) {
        stepped = ShellCustomizationEditor::previousPanelTarget(
            profile, m_keyboardTarget);
    }
    if (!stepped.has_value()) {
        return false;
    }
    m_keyboardTarget = *stepped;
    return hoverDropTarget(m_keyboardTarget.panelId,
                           m_keyboardTarget.zone,
                           m_keyboardTarget.beforeAppletId.value_or(QString{}));
}

QString CustomizeSettingsModel::nextDuplicateId(const QString &base) const
{
    const auto *profile = m_editor ? m_editor->profile() : nullptr;
    for (int suffix = 2; suffix <= 10'000; ++suffix) {
        const QString candidate = base + QStringLiteral("-copy-%1").arg(suffix);
        bool used = false;
        if (profile != nullptr) {
            for (const auto &panel : profile->panels) {
                used = appletById(&panel, candidate) != nullptr || used;
            }
        }
        if (!used) {
            return candidate;
        }
    }
    return {};
}

bool CustomizeSettingsModel::removeSelected()
{
    if (!canEdit() || m_selectedKind != QLatin1String("applet")) {
        return false;
    }
    const auto target = targetFromStrings(m_selectedPanelId,
                                          QStringLiteral("start"), {});
    return settleEditorOutcome(m_editor->applyGesture(
        ShellCustomizationEditor::removeIntent(m_selectedPanelId,
                                               m_selectedAppletId),
        target),
        QStringLiteral("Applet removed"));
}

bool CustomizeSettingsModel::duplicateSelected()
{
    if (!canEdit() || m_selectedKind != QLatin1String("applet")) {
        return false;
    }
    const QString duplicateId = nextDuplicateId(m_selectedAppletId);
    const auto target = targetFromStrings(m_selectedPanelId,
        selectedProperties().value(QStringLiteral("zone")).toString(), {});
    return !duplicateId.isEmpty()
        && settleEditorOutcome(m_editor->applyGesture(
            ShellCustomizationEditor::duplicateIntent(
                m_selectedPanelId, m_selectedAppletId, duplicateId),
            target, duplicateId), QStringLiteral("Applet duplicated"));
}

bool CustomizeSettingsModel::configureSelectedPanel(const QString &field,
                                                     const QVariant &value)
{
    if (!canEdit() || m_selectedKind != QLatin1String("panel")) {
        return false;
    }
    const auto *panel = panelById(m_editor->profile(), m_selectedPanelId);
    if (panel == nullptr) {
        return false;
    }
    auto configuration = panelConfiguration(*panel);
    if (field == QLatin1String("edge") || field == QLatin1String("alignment")) {
        Profiles::Edge edge = panel->edge;
        Profiles::Alignment alignment = panel->alignment;
        if (value.metaType().id() != QMetaType::QString
            || (field == QLatin1String("edge")
                    ? !Profiles::parseEdge(value.toString(), &edge)
                    : !Profiles::parseAlignment(value.toString(), &alignment))) {
            return false;
        }
        const auto intent = ShellCustomizationEditor::movePanelIntent(
            panel->id, panel->output, edge, alignment, std::nullopt);
        return settleEditorOutcome(m_editor->applyGesture(
            intent, targetFromStrings(panel->id, QStringLiteral("start"), {})));
    }
    if (field == QLatin1String("layer")) {
        if (value.metaType().id() != QMetaType::QString
            || !Profiles::parseLayer(value.toString(), &configuration.layer)) {
            return false;
        }
    } else if (field == QLatin1String("hideMode")) {
        if (value.metaType().id() != QMetaType::QString
            || !Profiles::parseHideMode(value.toString(),
                                        &configuration.hideMode)) {
            return false;
        }
    } else if (field == QLatin1String("rows")) {
        configuration.rows = value.toInt();
    } else if (field == QLatin1String("thickness")) {
        configuration.thickness = value.toInt();
    } else if (field == QLatin1String("length")) {
        configuration.length = value.toDouble();
    } else {
        return false;
    }
    return settleEditorOutcome(m_editor->applyGesture(
        ShellCustomizationEditor::configureIntent(panel->id, configuration),
        targetFromStrings(panel->id, QStringLiteral("start"), {})));
}

bool CustomizeSettingsModel::undo()
{
    return canUndo() && settleEditorOutcome(m_editor->undo(),
                                             QStringLiteral("Layout edit undone"));
}

bool CustomizeSettingsModel::redo()
{
    return canRedo() && settleEditorOutcome(m_editor->redo(),
                                             QStringLiteral("Layout edit redone"));
}

bool CustomizeSettingsModel::apply()
{
    if (!applyAvailable()) {
        return false;
    }
    if (m_editor->dirty() && !settleEditorOutcome(m_editor->apply())) {
        return false;
    }
    if (!m_selectionDirty) {
        setState(State::Ready);
        refreshProjection();
        return true;
    }
    setState(State::Saving);
    QString error;
    if (!m_client.setUserValue(QString(LayoutProfileSettingsKey),
                               m_selectedProfileId, &error)) {
        setState(State::Ready, error);
        return false;
    }
    return true;
}

bool CustomizeSettingsModel::discard()
{
    if (!m_hasBaseline || saving()) {
        return false;
    }
    if (visualDragActive()) {
        const auto outcome = m_editor->cancel();
        if (!outcome.ok) {
            return settleEditorOutcome(outcome);
        }
    }
    const auto *profile = findProfile(m_confirmedProfileId);
    if (profile == nullptr || !rebuild(*profile)) {
        return false;
    }
    m_selectionDirty = false;
    m_keyboardMoving = false;
    setState(State::Ready);
    refreshProjection();
    return true;
}

} // namespace QindaQt::Apps::SettingsCustomize
