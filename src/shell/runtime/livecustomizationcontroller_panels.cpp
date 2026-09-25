// SPDX-License-Identifier: GPL-3.0-or-later
// Panel-menu half of LiveCustomizationController (palette, add applet,
// panel options, add/remove panel, the desktop applet lookup); split from
// livecustomizationcontroller.cpp for source shape.
#include "livecustomizationcontroller.h"

#include "livecustomizationmodel.h"

#include "qindaqt/profiles/profile_types.h"
#include "qindaqt/shell_customization_editor/editor_intent.h"
#include "qindaqt/shell_customization_editor/editor_session.h"
#include "qindaqt/shell_customization_editor/live_editor_host.h"

#include <QMetaType>

namespace QindaQt::Shell {

namespace Model = LiveCustomizationModel;
using ShellCustomizationEditor::DropTarget;
using ShellCustomizationEditor::EditorOutcome;

namespace {

DropTarget targetOf(const QString &panelId, const QString &zone)
{
    DropTarget target;
    target.panelId = panelId;
    target.zone = zone;
    return target;
}

} // namespace

QVariantList LiveCustomizationController::palette(const QString &panelId) const
{
    const auto *owner = panel(panelId);
    return owner ? Model::paletteRows(m_manifests, *owner) : QVariantList{};
}

bool LiveCustomizationController::addApplet(const QString &panelId, const QString &zone,
                                            const QString &pluginId)
{
    if (!ensureHost()) {
        return false;
    }
    const QString instanceId = Model::nextInstanceId(*m_host->profile(), pluginId);
    return settle(QStringLiteral("add-applet"),
                  m_host->applyGesture(ShellCustomizationEditor::InsertAppletIntent{pluginId},
                                       targetOf(panelId, zone), instanceId),
                  true);
}

QVariantMap LiveCustomizationController::panelOptions(const QString &panelId) const
{
    const auto *owner = panel(panelId);
    return owner ? Model::panelOptions(*owner) : QVariantMap{};
}

QVariantList LiveCustomizationController::panelIds() const
{
    QVariantList ids;
    if (const auto *profile = m_host ? m_host->profile() : nullptr) {
        for (const auto &spec : profile->panels) {
            ids.append(spec.id);
        }
    }
    return ids;
}

bool LiveCustomizationController::configurePanel(const QString &panelId, const QString &field,
                                                 const QVariant &value)
{
    if (!ensureHost()) {
        return false;
    }
    const auto *spec = panel(panelId);
    if (spec == nullptr) {
        m_statusText = QStringLiteral("the panel is unknown");
        Q_EMIT changed();
        return false;
    }
    const DropTarget at = targetOf(panelId, QStringLiteral("start"));
    // The same field-to-intent split the retired Settings editor used:
    // edge/alignment/output restack through MovePanel, the other five replace
    // the whole ConfigurePanel tuple.
    if (field == QLatin1String("output")) {
        // "*" shows the panel on every display; an exact output id pins it to
        // that one (the menu offers the display the panel was clicked on).
        // The engine refuses an output the inventory does not have.
        const QString output = value.metaType().id() == QMetaType::QString ? value.toString()
                                                                           : QString();
        if (output.isEmpty()) {
            m_statusText = QStringLiteral("the display is unknown");
            Q_EMIT changed();
            return false;
        }
        if (output == spec->output) {
            return settle(QStringLiteral("panel-output"), EditorOutcome::success(), false);
        }
        return settle(QStringLiteral("panel-output"),
                      m_host->applyGesture(ShellCustomizationEditor::movePanelIntent(
                                               panelId, output, spec->edge, spec->alignment,
                                               std::nullopt),
                                           at),
                      true);
    }
    if (field == QLatin1String("edge") || field == QLatin1String("alignment")) {
        Profiles::Edge edge = spec->edge;
        Profiles::Alignment alignment = spec->alignment;
        const bool parsed = value.metaType().id() == QMetaType::QString
            && (field == QLatin1String("edge") ? Profiles::parseEdge(value.toString(), &edge)
                                               : Profiles::parseAlignment(value.toString(), &alignment));
        if (!parsed) {
            m_statusText = QStringLiteral("'%1' is not a valid %2").arg(value.toString(), field);
            Q_EMIT changed();
            return false;
        }
        if (edge == spec->edge && alignment == spec->alignment) {
            return settle(QStringLiteral("panel-") + field, EditorOutcome::success(), false);
        }
        return settle(QStringLiteral("panel-") + field,
                      m_host->applyGesture(ShellCustomizationEditor::movePanelIntent(
                                               panelId, spec->output, edge, alignment, std::nullopt),
                                           at),
                      true);
    }
    ShellCustomizationEditor::PanelConfiguration configuration{
        spec->layer, spec->hideMode, spec->rows, spec->thickness, spec->length};
    bool parsed = true;
    if (field == QLatin1String("layer")) {
        parsed = value.metaType().id() == QMetaType::QString
            && Profiles::parseLayer(value.toString(), &configuration.layer);
    } else if (field == QLatin1String("hideMode")) {
        parsed = value.metaType().id() == QMetaType::QString
            && Profiles::parseHideMode(value.toString(), &configuration.hideMode);
    } else if (field == QLatin1String("rows")) {
        configuration.rows = value.toInt();
    } else if (field == QLatin1String("thickness")) {
        configuration.thickness = value.toInt();
    } else if (field == QLatin1String("length")) {
        configuration.length = value.toDouble();
    } else {
        parsed = false;
    }
    if (!parsed) {
        m_statusText = QStringLiteral("'%1' is not a panel option").arg(field);
        Q_EMIT changed();
        return false;
    }
    return settle(QStringLiteral("panel-") + field,
                  m_host->applyGesture(
                      ShellCustomizationEditor::configureIntent(panelId, configuration), at),
                  true);
}

QString LiveCustomizationController::desktopAppletId(const QString &pluginId) const
{
    if (const auto *profile = m_host ? m_host->profile() : nullptr) {
        for (const auto &applet : profile->desktopApplets) {
            if (applet.plugin == pluginId) {
                return applet.id;
            }
        }
    }
    return {};
}

bool LiveCustomizationController::addPanel(const QString &edge)
{
    if (!ensureHost()) {
        return false;
    }
    Profiles::Edge parsedEdge = Profiles::Edge::Top;
    if (!Profiles::parseEdge(edge, &parsedEdge)) {
        m_statusText = QStringLiteral("'%1' is not a panel edge").arg(edge);
        Q_EMIT changed();
        return false;
    }
    const QString id = Model::nextPanelId(*m_host->profile());
    return settle(QStringLiteral("add-panel"),
                  m_host->applyGesture(ShellCustomizationEditor::addPanelIntent(
                                           Model::defaultPanel(id, parsedEdge), std::nullopt),
                                       targetOf(id, QStringLiteral("start"))),
                  true);
}

bool LiveCustomizationController::removePanel(const QString &panelId)
{
    if (!ensureHost()) {
        return false;
    }
    return settle(QStringLiteral("remove-panel"),
                  m_host->applyGesture(ShellCustomizationEditor::removePanelIntent(panelId),
                                       targetOf(panelId, QStringLiteral("start"))),
                  true);
}

} // namespace QindaQt::Shell
