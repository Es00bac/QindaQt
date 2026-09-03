// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "panelvisibilitytimer.h"

#include "qindaqt/shell_surface/panel_surface_configuration.h"
#include "qindaqt/shell_visibility/panel_visibility_types.h"

#include <QObject>

class QEvent;
class QGuiApplication;

namespace QindaQt::ShellOrchestration {
class PanelInteractionStore;
}

namespace QindaQt::Shell {

class PanelVisibilityPointerProducer final : public QObject {
    Q_OBJECT
public:
    PanelVisibilityPointerProducer(
        QGuiApplication &application,
        ShellOrchestration::PanelInteractionStore &interactions,
        PanelVisibilityTimerPort &timer, QObject *parent = nullptr);
    ~PanelVisibilityPointerProducer() override;

    void setIdentities(
        QVector<ShellVisibility::PanelSurfaceIdentity> identities);
    void setLeaveDelayMilliseconds(int delayMilliseconds);

    // These are the toolkit-adapter boundary and deterministic unit-test seam.
    void pointerEntered(const ShellVisibility::PanelSurfaceIdentity &identity);
    void pointerLeft(const ShellVisibility::PanelSurfaceIdentity &identity);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    class Private;
    Private *m_private = nullptr;
};

// Publishes one transparent, non-reserving logical pixel at the output edge
// for every hideable panel. It uses ShellSurface's public backend; no
// layer-shell policy or protocol object leaks into the interaction producer.
class PanelVisibilityEdgeSurfaceProducer final {
public:
    explicit PanelVisibilityEdgeSurfaceProducer(
        PanelVisibilityPointerProducer &pointer);
    ~PanelVisibilityEdgeSurfaceProducer();

    [[nodiscard]] bool synchronize(
        const ShellSurface::PanelSurfacePlan &panelPlan,
        const QVector<ShellVisibility::PanelSurfaceIdentity> &hideable,
        QString *error = nullptr);

private:
    class Private;
    Private *m_private = nullptr;
};

} // namespace QindaQt::Shell
