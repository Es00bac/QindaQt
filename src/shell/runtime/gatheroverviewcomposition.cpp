// SPDX-License-Identifier: GPL-3.0-or-later
#include "gatheroverviewcomposition.h"
#include "kwinscreenshotpreviewport.h"

#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"

#include <LayerShellQt/Window>

#include <QBuffer>
#include <QCursor>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QRectF>
#include <QScreen>
#include <QtLogging>

namespace QindaQt::Shell {

GatherOverviewComposition::GatherOverviewComposition(
    QGuiApplication &app, QQmlEngine &engine,
    ShellGatherOverview::GatherOverviewController::IconNameResolver
        iconNameResolver,
    QObject *parent)
    : QObject(parent), m_app(app), m_engine(engine),
      m_controller(
          std::make_unique<ShellGatherOverview::GatherOverviewController>(
              std::move(iconNameResolver))),
      m_previewPort(std::make_unique<KWinScreenshotPreviewPort>())
{
    connect(m_controller.get(),
            &ShellGatherOverview::GatherOverviewController::openChanged, this,
            [this] { publishOpenState(); });
    connect(m_controller.get(),
            &ShellGatherOverview::GatherOverviewController::projectionChanged,
            this, &GatherOverviewComposition::requestVisiblePreviews);
    connect(m_previewPort.get(),
            &ShellTaskListApplet::TaskListAppletPreviewPort::previewFinished,
            this, [this](const ShellTaskListApplet::TaskListPreviewResult &result) {
                if (!m_controller->isOpen() || !result.ok
                    || result.revision != m_previewRevision
                    || !m_requestedPreviews.contains(result.windowId)) {
                    return;
                }
                QByteArray png;
                QBuffer buffer(&png);
                if (!buffer.open(QIODevice::WriteOnly)
                    || !result.image.save(&buffer, "PNG")) {
                    return;
                }
                // Keep previews inside the audited shell process. The data
                // URLs disappear when Gather closes or the source changes;
                // no window pixels are written to disk.
                m_previewUrls.insert(
                    result.windowId,
                    QString::fromLatin1("data:image/png;base64,")
                        + QString::fromLatin1(png.toBase64()));
                publishPreviews();
            });
    connect(m_controller.get(),
            &ShellGatherOverview::GatherOverviewController::activationRequested,
            this, &GatherOverviewComposition::handleActivation);
    // An activation closes the overview. It is a temporary surface by design:
    // the user asked for one window and the arrangement has served its
    // purpose, and leaving it up over the window it just raised would hide
    // the thing the click was for.
    connect(m_controller.get(),
            &ShellGatherOverview::GatherOverviewController::activationRequested,
            this, [this] { m_controller->close(); });
    connect(&m_app, &QGuiApplication::screenAdded, this,
            [this](QScreen *screen) {
                if (m_started) {
                    createWindow(screen);
                }
            });
    connect(&m_app, &QGuiApplication::screenRemoved, this,
            [this](QScreen *screen) {
                if (m_openScreen.data() == screen) {
                    // The output the overview was on is gone. Close rather
                    // than migrate: the arrangement was planned against that
                    // work area, and a silent jump to another monitor is a
                    // worse surprise than a dismissal.
                    m_controller->close();
                }
                if (auto *window = m_windows.take(screen)) {
                    delete window;
                }
            });
}

GatherOverviewComposition::~GatherOverviewComposition()
{
    // The windows hold QML that reads the controller, so they go first.
    for (QQuickWindow *window : std::as_const(m_windows)) {
        delete window;
    }
    m_windows.clear();
}

void GatherOverviewComposition::setTaskListAccess(
    ShellTaskListApplet::TaskListAppletController *access)
{
    if (m_taskList == access) {
        return;
    }
    if (m_taskList) {
        disconnect(m_taskList, nullptr, this, nullptr);
    }
    m_taskList = access;
    if (!m_taskList) {
        // Nothing truthful left to project; an open overview must not keep
        // drawing a window list it can no longer confirm.
        m_controller->close();
        feedSource();
        return;
    }
    connect(m_taskList,
            &ShellTaskListApplet::TaskListAppletController::stateReprojected,
            this, &GatherOverviewComposition::feedSource);
    feedSource();
}

void GatherOverviewComposition::start()
{
    if (m_started) {
        return;
    }
    m_started = true;
    const QList<QScreen *> screens = m_app.screens();
    for (QScreen *screen : screens) {
        createWindow(screen);
    }
}

ShellGatherOverview::GatherOverviewController *
GatherOverviewComposition::controller() const noexcept
{
    return m_controller.get();
}

QObject *GatherOverviewComposition::controllerObject() const noexcept
{
    return m_controller.get();
}

int GatherOverviewComposition::surfaceCount() const noexcept
{
    return static_cast<int>(m_windows.size());
}

void GatherOverviewComposition::toggle()
{
    if (!m_taskList) {
        // Fail-closed: a denied or absent task list has no windows to gather.
        return;
    }
    m_controller->toggle();
}

void GatherOverviewComposition::close() { m_controller->close(); }

void GatherOverviewComposition::feedSource()
{
    if (!m_taskList) {
        m_controller->setSource({});
        return;
    }
    m_controller->setSource(m_taskList->projection());
}

void GatherOverviewComposition::handleActivation(const QString &taskId,
                                                 const QString &windowId,
                                                 quint64 generationRevision)
{
    if (!m_taskList) {
        return;
    }
    // The controller reports the identity and the generation it projected
    // from; the applet controller refuses a stale revision before dispatch.
    // An empty windowId addresses the whole entry, a non-empty one the member
    // - the same split the strip's ungrouped rows use.
    const bool dispatched =
        windowId.isEmpty()
            ? m_taskList->activateTask(taskId, generationRevision)
            : m_taskList->activateTaskWindow(taskId, windowId,
                                             generationRevision);
    if (!dispatched) {
        qWarning().noquote()
            << "QindaQt gather overview could not activate" << taskId
            << "at revision" << generationRevision;
    }
}

QScreen *GatherOverviewComposition::screenToOpenOn() const
{
    // The output the pointer is on is what a corner brush and an edge swipe
    // both mean, and it is the best available guess for Meta+G too: a
    // layer-shell client is never told which output has keyboard focus.
    // screenAt returns null where the platform will not report a global
    // cursor position, which is why the primary screen is the fallback rather
    // than the assumption.
    if (QScreen *under = m_app.screenAt(QCursor::pos())) {
        return under;
    }
    return m_app.primaryScreen();
}

void GatherOverviewComposition::publishOpenState()
{
    const bool open = m_controller->isOpen();
    if (open && m_openScreen.isNull()) {
        m_openScreen = screenToOpenOn();
    }
    for (auto it = m_windows.cbegin(); it != m_windows.cend(); ++it) {
        QQuickWindow *window = it.value();
        if (window == nullptr) {
            continue;
        }
        const bool wanted = open && it.key() == m_openScreen.data();
        if (auto *layer = LayerShellQt::Window::get(window)) {
            // The keyboard is taken only while the overview is up, and only
            // by the one surface that is up. A surface that holds it while
            // hidden swallows every keystroke in the session, which is the
            // worst failure this feature could have.
            layer->setKeyboardInteractivity(
                wanted ? LayerShellQt::Window::KeyboardInteractivityExclusive
                       : LayerShellQt::Window::KeyboardInteractivityNone);
        }
        window->setVisible(wanted);
    }
    if (open) {
        // The work area can have changed while the overview was down - an
        // output added, a panel resized - and the planner reads it at
        // projection time, so it is read here rather than cached at startup.
        QScreen *screen = m_openScreen.data();
        if (screen != nullptr) {
            m_controller->setWorkArea(QRectF(screen->availableGeometry()));
            if (QQuickWindow *window = m_windows.value(screen)) {
                // The surface draws a frame at `frame - origin`, so a window
                // on an output that does not start at (0,0) needs its own
                // origin or every tile lands off-screen.
                window->setProperty(
                    "workArea", QVariant::fromValue(
                                    QRectF(screen->availableGeometry())));
            }
        }
        requestVisiblePreviews();
    } else {
        clearPreviews();
        m_openScreen.clear();
    }
}

void GatherOverviewComposition::requestVisiblePreviews()
{
    if (!m_controller->isOpen() || !m_previewPort)
        return;
    const QVariantMap projection = m_controller->projection();
    if (!projection.value(QStringLiteral("interactive")).toBool()) {
        clearPreviews();
        return;
    }
    const QVariantList items = projection.value(QStringLiteral("items")).toList();
    if (items.isEmpty()) {
        clearPreviews();
        return;
    }
    const quint64 visibleRevision =
        items.constFirst().toMap().value(QStringLiteral("generationRevision"))
            .toULongLong();
    if (visibleRevision == 0
        || (m_previewRevision != 0 && m_previewRevision != visibleRevision)) {
        clearPreviews();
    }
    for (const QVariant &value : items) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("lane")).toString()
                != QLatin1StringView("window")) {
            continue;
        }
        // AGENT-CONTRACT: a standalone task's taskId is its compositor window
        // UUID; windowId is populated only for an ungrouped container member.
        // Do not change the activation identity to make previews work.
        QString windowId = item.value(QStringLiteral("windowId")).toString();
        if (windowId.isEmpty())
            windowId = item.value(QStringLiteral("taskId")).toString();
        const quint64 revision =
            item.value(QStringLiteral("generationRevision")).toULongLong();
        if (windowId.isEmpty() || revision == 0)
            continue;
        if (m_previewRevision != revision) {
            clearPreviews();
            m_previewRevision = revision;
        }
        if (m_requestedPreviews.contains(windowId))
            continue;
        m_requestedPreviews.insert(windowId);
        const QSize size = item.value(QStringLiteral("frame"))
                               .toRectF().size().toSize()
                               .boundedTo(QSize(1024, 1024))
                               .expandedTo(QSize(16, 16));
        m_previewPort->requestPreview({windowId, revision, size});
    }
}

void GatherOverviewComposition::clearPreviews()
{
    if (m_previewPort)
        m_previewPort->cancelAll();
    m_previewRevision = 0;
    m_requestedPreviews.clear();
    m_previewUrls.clear();
    publishPreviews();
}

void GatherOverviewComposition::publishPreviews()
{
    for (QQuickWindow *window : std::as_const(m_windows)) {
        if (window)
            window->setProperty("previewUrls", m_previewUrls);
    }
}

void GatherOverviewComposition::createWindow(QScreen *screen)
{
    if (screen == nullptr || m_windows.contains(screen)) {
        return;
    }
    QQmlComponent component(&m_engine);
    component.loadFromModule(QStringLiteral("QindaQt.Shell.GatherOverview"),
                             QStringLiteral("GatherOverviewWindow"));
    if (component.isError()) {
        qWarning().noquote()
            << "QindaQt gather overview window failed to load:"
            << component.errorString().trimmed();
        return;
    }
    QObject *object = component.createWithInitialProperties(
        {{QStringLiteral("controller"),
          QVariant::fromValue(static_cast<QObject *>(m_controller.get()))},
         {QStringLiteral("workArea"),
          QVariant::fromValue(QRectF(screen->availableGeometry()))},
         {QStringLiteral("previewUrls"), m_previewUrls}});
    auto *raw = qobject_cast<QQuickWindow *>(object);
    if (raw == nullptr) {
        qWarning().noquote()
            << "QindaQt gather overview component did not create a window";
        delete object;
        return;
    }
    raw->setScreen(screen);
    raw->setFlags(Qt::FramelessWindowHint);
    auto *layer = LayerShellQt::Window::get(raw);
    if (layer == nullptr) {
        delete raw;
        return;
    }
    layer->setWantsToBeOnActiveScreen(false);
    layer->setScreen(screen);
    // AGENT-CONTRACT: the `desktop` scope is reserved for the wallpaper and
    // the desktop-icon surface - KWin maps exactly that scope to
    // WindowType::Desktop. This surface must NOT claim it: it sits above
    // windows, not behind them. Any other scope becomes a Normal window to
    // KWin, so it would appear in the very task list it is drawing; the
    // overlay layer plus a zero exclusive zone is what keeps it out of the
    // way, and the task-list producer filters layer surfaces already.
    layer->setScope(QStringLiteral("gather-overview"));
    LayerShellQt::Window::Anchors anchors = LayerShellQt::Window::AnchorTop;
    anchors |= LayerShellQt::Window::AnchorBottom;
    anchors |= LayerShellQt::Window::AnchorLeft;
    anchors |= LayerShellQt::Window::AnchorRight;
    layer->setAnchors(anchors);
    layer->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityNone);
    layer->setLayer(LayerShellQt::Window::LayerOverlay);
    // Zero, not -1: the overview covers the panel while it is up but must
    // never reserve space, or every window in the session would be resized
    // the first time it opened.
    layer->setExclusiveZone(0);
    // AGENT-GUARD: false, deliberately. closeOnDismissed makes LayerShellQt
    // call close() on the window itself, which leaves the controller still
    // believing the overview is up - and the controller is the only thing the
    // shortcut, the applet and the corner all read, so a desync there means
    // the next Meta+G closes an overview the user cannot see. Dismissal
    // reaches the controller through the surface's own dismissRequested
    // (Escape, or a click that missed every tile) instead.
    layer->setCloseOnDismissed(false);
    layer->setDesiredSize(QSize(0, 0));
    m_windows.insert(screen, raw);
}

} // namespace QindaQt::Shell
