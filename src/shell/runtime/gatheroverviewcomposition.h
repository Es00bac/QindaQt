// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "gatherpreviewledger.h"
#include "qindaqt/shell/gather_overview/gather_overview_controller.h"

#include <QHash>
#include <QObject>
#include <QPointer>

#include <memory>

class QGuiApplication;
class QQmlEngine;
class QQuickWindow;
class QScreen;

namespace QindaQt::ShellTaskListApplet {
class TaskListAppletController;
}

namespace QindaQt::Shell {

class KWinScreenshotPreviewPort;

// Owns the gather overview (ADR-0232): its controller, its per-output overlay
// windows,
// and the ways it is reached - the Meta+G shortcut, the panel applet button,
// and the upper-left screen corner KWin no longer reserves.
//
// AGENT-CONTRACT: the task-list applet controller is borrowed and must
// outlive this composition. Everything the overview knows about windows it
// learns from that controller's projection, and every action it takes goes
// back through that controller's task intents, so it inherits the applet's
// grants and its stale-revision arbitration rather than reimplementing
// either. A null controller yields an overview that never opens, which is
// right for a session whose task-list grants were denied: there is nothing
// truthful to show and nothing it would be allowed to do.
//
// AGENT-CONTRACT: overlay windows are created once per output and then shown
// and hidden. See GatherOverviewWindow.qml for why not per invocation.
class GatherOverviewComposition final : public QObject {
    Q_OBJECT
    // The panel applet reads `access.controller.open` to draw its pressed
    // state. CONSTANT because the composition owns exactly one controller for
    // its whole life; the controller's own `open` property carries the change.
    Q_PROPERTY(QObject *controller READ controllerObject CONSTANT)

public:
    GatherOverviewComposition(
        QGuiApplication &app, QQmlEngine &engine,
        ShellGatherOverview::GatherOverviewController::IconNameResolver
            iconNameResolver,
        QObject *parent = nullptr);
    ~GatherOverviewComposition() override;

    GatherOverviewComposition(const GatherOverviewComposition &) = delete;
    GatherOverviewComposition &operator=(const GatherOverviewComposition &) = delete;

    // Borrowed; may be null. Connects the projection feed and the intent path.
    void setTaskListAccess(ShellTaskListApplet::TaskListAppletController *access);

    // Creates the overlay windows. Until this is called the overview has no
    // surface, so a preview or a measuring session costs nothing.
    void start();

    [[nodiscard]] ShellGatherOverview::GatherOverviewController *
    controller() const noexcept;
    // Same object as controller(), typed for the Q_PROPERTY above.
    [[nodiscard]] QObject *controllerObject() const noexcept;
    [[nodiscard]] int surfaceCount() const noexcept;

public Q_SLOTS:
    // The one door. Every entry point comes through here, so they can never
    // disagree about whether the overview is up.
    void toggle();
    void close();

private:
    void createWindow(QScreen *screen);
    [[nodiscard]] QScreen *screenToOpenOn() const;
    void publishOpenState();
    void feedSource();
    void handleActivation(const QString &taskId, const QString &windowId,
                          quint64 generationRevision);
    void requestVisiblePreviews();
    void clearPreviews();
    void publishPreviews();

    QGuiApplication &m_app;
    QQmlEngine &m_engine;
    QPointer<ShellTaskListApplet::TaskListAppletController> m_taskList;
    std::unique_ptr<ShellGatherOverview::GatherOverviewController> m_controller;
    std::unique_ptr<KWinScreenshotPreviewPort> m_previewPort;
    GatherPreviewLedger m_previewLedger;
    QHash<QScreen *, QQuickWindow *> m_windows;
    // AGENT-CONTRACT: the overview is ONE arrangement, shown on one output.
    // There is a window per output so that whichever screen the user is on
    // can host it without a roundtrip, but exactly one is ever visible and
    // the controller carries that screen's work area. Showing them all would
    // draw the chosen screen's arrangement on every monitor, since the
    // controller holds a single work area and a single projection.
    QPointer<QScreen> m_openScreen;
    bool m_started = false;
};

} // namespace QindaQt::Shell
