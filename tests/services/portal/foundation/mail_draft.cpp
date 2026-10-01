// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic native application endpoint, not an installed mailer or sender.
// Its desktop Exec receives the real URI opener's argv and selected ordinary
// FD; the mapped-window evidence is recorded privately, never printed.
#include <QGuiApplication>
#include <QQuickWindow>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUrl>
#include <QtGui/qguiapplication_platform.h>
#include <wayland-client.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>
int main(int argc, char **argv) {
    rlimit cores{0, 0}; if (setrlimit(RLIMIT_CORE, &cores) != 0 || prctl(PR_SET_DUMPABLE, 0) != 0) return 2;
    QGuiApplication app(argc, argv); if (app.arguments().size() != 2) return 2;
    const QString argument = app.arguments().at(1); const QUrl uri(argument, QUrl::StrictMode);
    if (!uri.isValid() || uri.scheme() != QStringLiteral("mailto")) return 2;
    QQuickWindow window; window.setTitle(QStringLiteral("Private draft fixture")); window.resize(420, 160); window.show();
    QTimer poll; poll.setInterval(50);
    QObject::connect(&poll, &QTimer::timeout, &app, [&] {
        if (!window.isExposed()) return;
        auto *native = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
        ucred peer{}; socklen_t length = sizeof(peer);
        if (!native || getsockopt(wl_display_get_fd(native->display()), SOL_SOCKET, SO_PEERCRED, &peer, &length) != 0
            || peer.pid != qEnvironmentVariableIntValue("QINDAQT_PORTAL_TEST_COMPOSITOR_PID")) { app.exit(3); return; }
        QFile audit(qEnvironmentVariable("QINDAQT_PORTAL_TEST_MAIL_AUDIT"));
        if (!audit.open(QIODevice::WriteOnly)) { app.exit(4); return; }
        audit.write(QJsonDocument(QJsonObject{{QStringLiteral("uri"), argument}, {QStringLiteral("mapped"), true},
            {QStringLiteral("exactPeer"), true}}).toJson(QJsonDocument::Compact)); audit.close();
        poll.stop(); QTimer::singleShot(100, &app, &QCoreApplication::quit);
    });
    poll.start(); QTimer::singleShot(5000, &app, [&] { app.exit(5); }); return app.exec();
}
