// SPDX-License-Identifier: GPL-3.0-or-later
// Edit-mode drags and solved-surface geometry of LiveCustomizationController
// (split from livecustomizationcontroller.cpp for source shape).
#include "livecustomizationcontroller.h"

#include "livecustomizationmodel.h"

#include "qindaqt/shell_customization_editor/editor_intent.h"
#include "qindaqt/shell_customization_editor/editor_session.h"
#include "qindaqt/shell_customization_editor/live_editor_host.h"

#include <QPoint>

namespace QindaQt::Shell {

namespace Model = LiveCustomizationModel;
using ShellCustomizationEditor::DropTarget;
using ShellCustomizationEditor::EditorOutcome;

namespace {

DropTarget targetOf(const QString &panelId, const QString &zone, const QString &before = {})
{
    DropTarget target;
    target.panelId = panelId;
    target.zone = zone;
    if (!before.isEmpty()) {
        target.beforeAppletId = before;
    }
    return target;
}

QVariantMap surfaceMap(const ShellLayout::PanelSurface &surface)
{
    const bool horizontal = surface.edge == Profiles::Edge::Top
        || surface.edge == Profiles::Edge::Bottom;
    return {{QStringLiteral("panelId"), surface.panelId},
            {QStringLiteral("outputId"), surface.outputId},
            {QStringLiteral("x"), surface.geometry.x()},
            {QStringLiteral("y"), surface.geometry.y()},
            {QStringLiteral("width"), surface.geometry.width()},
            {QStringLiteral("height"), surface.geometry.height()},
            {QStringLiteral("horizontal"), horizontal}};
}

} // namespace

bool LiveCustomizationController::beginAppletDrag(const QString &panelId,
                                                  const QString &appletId)
{
    if (!ensureHost()) {
        return false;
    }
    const auto *owner = panel(panelId);
    const auto *applet = owner ? Model::findApplet(*owner, appletId) : nullptr;
    if (applet == nullptr) {
        m_statusText = QStringLiteral("the dragged applet is unknown");
        Q_EMIT changed();
        return false;
    }
    ShellCustomizationEditor::DragPayload payload;
    payload.kind = ShellCustomizationEditor::PayloadKind::AppletInstance;
    payload.pluginId = applet->plugin;
    payload.sourcePanelId = panelId;
    payload.sourceAppletId = appletId;
    m_dropAccepted = false;
    m_dropReason.clear();
    const EditorOutcome armed = m_host->arm(payload);
    if (!armed.ok) {
        return settle(QStringLiteral("drag-arm"), armed, false);
    }
    return settle(QStringLiteral("drag-begin"), m_host->beginDrag(), false);
}

bool LiveCustomizationController::hoverDropTarget(const QString &panelId, const QString &zone,
                                                  const QString &beforeAppletId)
{
    if (!dragActive()) {
        return false;
    }
    const EditorOutcome outcome = m_host->hover(targetOf(panelId, zone, beforeAppletId));
    const auto acceptance = m_host->acceptance();
    m_dropAccepted = outcome.ok && (!acceptance.has_value() || acceptance->accepted);
    m_dropReason = acceptance.has_value() ? acceptance->reason : outcome.message;
    Q_EMIT changed();
    return outcome.ok;
}

bool LiveCustomizationController::dropApplet()
{
    if (!dragActive()) {
        return false;
    }
    const bool dropped = settle(QStringLiteral("drag-drop"), m_host->drop(), true);
    m_dropAccepted = false;
    m_dropReason.clear();
    Q_EMIT changed();
    return dropped;
}

bool LiveCustomizationController::cancelDrag()
{
    if (!m_host) {
        return false;
    }
    const bool cancelled = settle(QStringLiteral("drag-cancel"), m_host->cancel(), false);
    m_dropAccepted = false;
    m_dropReason.clear();
    Q_EMIT changed();
    return cancelled;
}

QVariantMap LiveCustomizationController::panelSurfaceAt(const QString &outputId, double x,
                                                        double y) const
{
    const auto *layout = m_host ? m_host->layout() : nullptr;
    if (layout == nullptr) {
        return {};
    }
    const QPoint point(static_cast<int>(x), static_cast<int>(y));
    for (const auto &surface : layout->surfaces) {
        if (surface.outputId == outputId && surface.geometry.contains(point)) {
            return surfaceMap(surface);
        }
    }
    return {};
}

QVariantMap LiveCustomizationController::panelSurface(const QString &outputId,
                                                      const QString &panelId) const
{
    const auto *layout = m_host ? m_host->layout() : nullptr;
    if (layout == nullptr) {
        return {};
    }
    for (const auto &surface : layout->surfaces) {
        if (surface.outputId == outputId && surface.panelId == panelId) {
            return surfaceMap(surface);
        }
    }
    return {};
}

} // namespace QindaQt::Shell
