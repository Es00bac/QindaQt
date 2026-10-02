// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QFile>
#include <QTimer>
#include <QtTest/QTest>
#include <QtGui/qguiapplication_platform.h>
#include <wayland-client.h>
#include <sys/socket.h>
#include <unistd.h>
namespace {
// Test-only visible input driver linked with the unchanged consent main,
// controller and QML. It can never be selected by a portal option. Approval
// is the production button/controller protocol; there is no scripted response.
void input() {
    for (auto *base : QGuiApplication::allWindows()) {
        auto *window = qobject_cast<QQuickWindow *>(base);
        if (!window || window->objectName() != QStringLiteral("portalConsentWindow") || !window->isExposed()) continue;
        auto *button = window->findChild<QQuickItem *>(QStringLiteral("portalGrantButton"));
        if (!button || !button->isVisible() || !button->isEnabled()) continue;
        auto *native = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
        ucred peer{}; socklen_t length = sizeof(peer);
        if (!native || getsockopt(wl_display_get_fd(native->display()), SOL_SOCKET, SO_PEERCRED, &peer, &length) != 0
            || peer.pid != qEnvironmentVariableIntValue("QINDAQT_PORTAL_TEST_COMPOSITOR_PID")) {
            QCoreApplication::exit(3); return;
        }
        QFile audit(qEnvironmentVariable("QINDAQT_PORTAL_TEST_AUDIT"));
        if (!audit.open(QIODevice::WriteOnly | QIODevice::Append)) { QCoreApplication::exit(4); return; }
        const auto mode = qEnvironmentVariable("QINDAQT_PORTAL_TEST_MODE");
        audit.write(QByteArray::number(getpid()) + " ordinary-wayland exact-peer mapped " + mode.toUtf8() + "\n"); audit.close();
        if (mode == QStringLiteral("hold")) return;
        if (mode == QStringLiteral("deny")) button = window->findChild<QQuickItem *>(QStringLiteral("portalDenyButton"));
        // grant-choices: click every visible unchecked boolean choice first,
        // through the same production QML toggle handler a user would use.
        if (mode == QStringLiteral("grant-choices")) {
            for (auto *item : window->contentItem()->findChildren<QQuickItem *>()) {
                if (!QByteArray(item->metaObject()->className()).contains("CheckBox") || !item->isVisible()
                    || item->property("checked").toBool()) continue;
                QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                    item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
            }
        }
        if (!button || !button->isVisible()) { QCoreApplication::exit(4); return; }
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
            button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint());
        return;
    }
    QTimer::singleShot(100, qApp, input);
}
void start() { QTimer::singleShot(100, QCoreApplication::instance(), input); }
Q_COREAPP_STARTUP_FUNCTION(start)
}
