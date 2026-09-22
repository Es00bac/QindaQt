// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell_surface/panel_surface_configuration.h"
#include "qindaqt/shell_visibility/panel_visibility_types.h"

#include <QObject>

#include <functional>

class QGuiApplication;
class QWindow;

namespace QindaQt::ShellOrchestration {
class PanelInteractionStore;
}

namespace QindaQt::Shell {

// AGENT-CONTRACT: the port owns *what* is faded. Callers name a panel window
// and a 0..1 progression; they must not set opacity on the QWindow themselves,
// because the production port does not fade the window (the Wayland platform
// implements no window opacity - see ADR-0236) and a caller that reset the
// wrong property would strand a panel part-faded.
class PanelVisibilityAnimationPort {
public:
    virtual ~PanelVisibilityAnimationPort() = default;
    virtual void animate(QWindow &window, qreal from, qreal to,
                         int durationMilliseconds,
                         std::function<void()> completed) = 0;
    virtual void cancel(QWindow &window) = 0;
    // Stops any running fade and returns the surface to fully opaque. This is
    // the only supported way to abandon a transition: cancel() alone leaves
    // the surface at whatever partial opacity it had reached.
    virtual void restore(QWindow &window) = 0;
};

class QtPanelVisibilityAnimation final : public QObject,
                                         public PanelVisibilityAnimationPort {
    Q_OBJECT
public:
    explicit QtPanelVisibilityAnimation(QObject *parent = nullptr);
    ~QtPanelVisibilityAnimation() override;

    void animate(QWindow &window, qreal from, qreal to,
                 int durationMilliseconds,
                 std::function<void()> completed) override;
    void cancel(QWindow &window) override;
    void restore(QWindow &window) override;

private:
    class Private;
    Private *m_private = nullptr;
};

class PanelVisibilityAnimationProducer final : public QObject {
    Q_OBJECT
public:
    PanelVisibilityAnimationProducer(
        QGuiApplication &application,
        ShellOrchestration::PanelInteractionStore &interactions,
        PanelVisibilityAnimationPort &animation,
        QObject *parent = nullptr);
    ~PanelVisibilityAnimationProducer() override;

    // Returns true only when a just-hidden surface received an animation hold
    // and the caller must immediately reevaluate the existing pure policy.
    [[nodiscard]] bool synchronize(
        const ShellSurface::PanelSurfacePlan &plan,
        const QVector<ShellVisibility::PanelSurfaceIdentity> &hideable,
        bool compositorAuthorityAvailable, int durationMilliseconds);

Q_SIGNALS:
    void reconcileRequested();

private:
    class Private;
    Private *m_private = nullptr;
};

} // namespace QindaQt::Shell
