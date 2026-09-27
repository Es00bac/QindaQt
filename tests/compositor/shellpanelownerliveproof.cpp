// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellpanelownerliveproof.h"

#include <LayerShellQt/Window>
#include <QBackingStore>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QProcess>
#include <QThread>
#include <QWindow>

namespace QindaQt::Compositor::TestSupport {
namespace {
constexpr auto Service = "org.qindaqt.Compositor";
constexpr auto Path = "/org/qindaqt/CompositorShell";
constexpr auto Interface = "org.qindaqt.CompositorShell1";

void dispatchFor(int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(2);
    }
}

QString snapshotStatus(const QString &method)
{
    QDBusInterface endpoint(QString::fromLatin1(Service), QString::fromLatin1(Path),
                            QString::fromLatin1(Interface));
    const QDBusReply<QByteArray> reply = endpoint.call(method);
    return reply.isValid() ? QJsonDocument::fromJson(reply.value()).object()
                                .value(QStringLiteral("status")).toString() : QString{};
}

bool awaitAuthority(bool authorized)
{
    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto identity = snapshotStatus(QStringLiteral("ActiveWindowIdentity"));
        const auto tasks = snapshotStatus(QStringLiteral("TaskListSnapshot"));
        if (!identity.isEmpty() && !tasks.isEmpty()
            && (identity != QStringLiteral("unauthorized")) == authorized
            && (tasks != QStringLiteral("unauthorized")) == authorized) {
            return true;
        }
        dispatchFor(10);
    }
    return false;
}

void paint(QWindow &window, QBackingStore &store, int frame)
{
    store.resize(window.size());
    const QRect area(QPoint{}, window.size());
    store.beginPaint(area);
    QPainter painter(store.paintDevice());
    painter.fillRect(area, frame % 2 ? Qt::blue : Qt::green);
    painter.end();
    store.endPaint();
    store.flush(area);
}

void mapPanel(QWindow &panel)
{
    panel.setSurfaceType(QSurface::RasterSurface);
    auto *layer = LayerShellQt::Window::get(&panel);
    layer->setScope(QStringLiteral("dock"));
    layer->setLayer(LayerShellQt::Window::LayerTop);
    layer->setDesiredSize(QSize(32, 24));
    layer->setExclusiveZone(0);
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    panel.show();
}

class Invalidations final : public QObject {
    Q_OBJECT
public:
    int identity = 0;
    int tasks = 0;
public Q_SLOTS:
    void identityChanged() { ++identity; }
    void tasksChanged() { ++tasks; }
};
} // namespace

bool provePanelOwnerFeedback(QWindow &panel, QString *failure)
{
    const auto fail = [failure](const QString &reason) {
        *failure = reason;
        return false;
    };
    if (!awaitAuthority(true)) return fail(QStringLiteral("initial panel not authorized"));
    Invalidations invalidations;
    auto bus = QDBusConnection::sessionBus();
    if (!bus.connect(QString::fromLatin1(Service), QString::fromLatin1(Path),
                     QString::fromLatin1(Interface), QStringLiteral("ActiveWindowIdentityChanged"),
                     &invalidations, SLOT(identityChanged()))
        || !bus.connect(QString::fromLatin1(Service), QString::fromLatin1(Path),
                         QString::fromLatin1(Interface), QStringLiteral("TaskListSnapshotChanged"),
                         &invalidations, SLOT(tasksChanged()))) {
        return fail(QStringLiteral("could not observe directed invalidations"));
    }
    QBackingStore store(&panel);
    paint(panel, store, 0);
    dispatchFor(300); // Let initial map/focus changes settle before counting.
    invalidations.identity = invalidations.tasks = 0;
    for (int frame = 0; frame < 80; ++frame) {
        paint(panel, store, frame);
        dispatchFor(15);
    }
    dispatchFor(100);
    {
        QWindow additionalPanel;
        mapPanel(additionalPanel);
        dispatchFor(100);
        QBackingStore additionalStore(&additionalPanel);
        paint(additionalPanel, additionalStore, 0);
        dispatchFor(100);
    }
    dispatchFor(100);
    if (invalidations.identity || invalidations.tasks) {
        return fail(QStringLiteral("80 repaints and a same-owner panel add/remove produced %1 identity / %2 task invalidations")
                        .arg(invalidations.identity).arg(invalidations.tasks));
    }
    // A conflicting committed dock must revoke authority, then disconnection
    // must recover the original owner even without a fresh original repaint.
    QProcess peer;
    peer.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--panel-owner-peer")});
    if (!peer.waitForStarted(2000) || !awaitAuthority(false)) {
        peer.kill();
        peer.waitForFinished(1000);
        return fail(QStringLiteral("conflicting dock owner did not revoke authority"));
    }
    peer.terminate();
    if (!peer.waitForFinished(2000) || !awaitAuthority(true)) {
        return fail(QStringLiteral("owner did not recover after conflicting client teardown"));
    }
    panel.destroy();
    if (!awaitAuthority(false)) return fail(QStringLiteral("last panel destruction retained authority"));
    panel.show();
    dispatchFor(100);
    paint(panel, store, 0);
    if (!awaitAuthority(true)) return fail(QStringLiteral("recreated panel failed to recover authority"));
    return true;
}

int runPanelOwnerPeer(QGuiApplication &application)
{
    QWindow panel;
    mapPanel(panel);
    return application.exec();
}
} // namespace QindaQt::Compositor::TestSupport
#include "shellpanelownerliveproof.moc"
