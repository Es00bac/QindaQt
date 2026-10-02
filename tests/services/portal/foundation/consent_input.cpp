// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <qindaqt/compositor_names/compositor_names.h>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDir>
#include <QQuickItem>
#include <QPointer>
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
QList<QQuickItem *> visualItems(QQuickItem *root) {
    QList<QQuickItem *> result;
    for (auto *child : root->childItems()) {
        result << child;
        result += visualItems(child);
    }
    return result;
}
void input() {
    for (auto *base : QGuiApplication::allWindows()) {
        auto *window = qobject_cast<QQuickWindow *>(base);
        if (!window || window->objectName() != QStringLiteral("portalConsentWindow") || !window->isExposed()) continue;
        auto *button = window->findChild<QQuickItem *>(QStringLiteral("portalGrantButton"));
        if (!button || !button->isVisible() || !button->isEnabled()) continue;
        auto *native = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
        ucred peer{}; socklen_t length = sizeof(peer);
        // AGENT-GUARD: the protected broker sanitizes all test env fields.
        // Authenticate its actual display against the current unique native
        // owner; any explicit historical expected PID remains an extra check.
        auto *bus = QDBusConnection::sessionBus().interface();
        if (!bus) { QCoreApplication::exit(3); return; }
        const auto owner = bus->serviceOwner(QString(QindaQt::CompositorNames::service));
        if (!owner.isValid() || !owner.value().startsWith(QLatin1Char(':'))) { QCoreApplication::exit(3); return; }
        const auto pid = bus->servicePid(owner.value());
        const bool expectedProvided = qEnvironmentVariableIsSet("QINDAQT_PORTAL_TEST_COMPOSITOR_PID");
        const auto expected = qEnvironmentVariableIntValue("QINDAQT_PORTAL_TEST_COMPOSITOR_PID");
        if (!native || !pid.isValid() || !pid.value()
            || getsockopt(wl_display_get_fd(native->display()), SOL_SOCKET, SO_PEERCRED, &peer, &length) != 0
            || peer.pid <= 0 || static_cast<quint32>(peer.pid) != pid.value()
            || (expectedProvided && (expected <= 0 || peer.pid != expected))) {
            QCoreApplication::exit(3); return;
        }
        const QString auditPath = qEnvironmentVariableIsSet("QINDAQT_PORTAL_TEST_AUDIT")
            ? qEnvironmentVariable("QINDAQT_PORTAL_TEST_AUDIT")
            : QDir(qEnvironmentVariable("XDG_RUNTIME_DIR")).filePath(QStringLiteral("qindaqt-consent.audit"));
        QFile audit(auditPath);
        if (!audit.open(QIODevice::WriteOnly | QIODevice::Append)) { QCoreApplication::exit(4); return; }
        const auto mode = qEnvironmentVariable("QINDAQT_PORTAL_TEST_MODE", QStringLiteral("grant-choices"));
        audit.write(QByteArray::number(getpid()) + " ordinary-wayland exact-peer mapped " + mode.toUtf8() + "\n"); audit.close();
        if (mode == QStringLiteral("hold")) return;
        if (mode == QStringLiteral("deny")) button = window->findChild<QQuickItem *>(QStringLiteral("portalDenyButton"));
        // grant-choices: click every visible unchecked boolean choice first,
        // through the same production QML toggle handler a user would use.
        if (mode == QStringLiteral("grant-choices")) {
            int clicked = 0, offered = 0;
            // Repeater delegates follow the visual parent tree; QObject
            // ownership can remain with their QML delegate model.
            QList<QPointer<QQuickItem>> items;
            for (auto *item : visualItems(window->contentItem())) items.append(item);
            for (const auto &guard : std::as_const(items)) {
                auto *item = guard.data();
                if (!item || !QByteArray(item->metaObject()->className()).contains("CheckBox") || !item->isVisible()) continue;
                ++offered;
                if (item->property("checked").toBool()) continue;
                QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                    item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
                ++clicked;
            }
            const bool selected = offered == 0 || QTest::qWaitFor([&] {
                for (auto *item : visualItems(window->contentItem()))
                    if (QByteArray(item->metaObject()->className()).contains("CheckBox") && item->isVisible()
                        && item->property("checked").toBool()) return true;
                return false;
            }, 1000);
            if (!audit.open(QIODevice::WriteOnly | QIODevice::Append)) { QCoreApplication::exit(4); return; }
            audit.write("offered-choices=" + QByteArray::number(offered) + " choice-clicks=" + QByteArray::number(clicked) + " selected=" + (selected ? "true\n" : "false\n"));
            audit.close();
            if (!selected) { QCoreApplication::exit(5); return; }
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
