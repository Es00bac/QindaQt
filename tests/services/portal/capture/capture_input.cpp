// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QFile>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QWindow>
#include <QtGui/qguiapplication_platform.h>
#include <QtTest/QTest>
#include <wayland-client.h>
#include <sys/socket.h>
#include <unistd.h>
namespace {
bool audited = false, allowed = false;
void input() {
    const auto action = qEnvironmentVariable("QINDAQT_CAPTURE_TEST_ACTION");
    for (auto *window : QApplication::topLevelWidgets()) {
        if (window->objectName() != "nativeCaptureDialog" || !window->isVisible() || !window->windowHandle() || !window->windowHandle()->isExposed()) continue;
        if (!audited) {
            auto *native = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>(); ucred peer{}; socklen_t size = sizeof(peer);
            if (!native || getsockopt(wl_display_get_fd(native->display()), SOL_SOCKET, SO_PEERCRED, &peer, &size) != 0 || peer.pid != qEnvironmentVariableIntValue("QINDAQT_PORTAL_TEST_COMPOSITOR_PID")) { QCoreApplication::exit(3); return; }
            QFile audit(qEnvironmentVariable("QINDAQT_CAPTURE_TEST_AUDIT")); if (!audit.open(QIODevice::WriteOnly | QIODevice::Append)) { QCoreApplication::exit(4); return; }
            audit.write(QByteArray::number(getpid()) + " ordinary exact-peer mapped\n"); audited = true;
        }
        if (action == "hold") return;
        if (action == "cancel") { if (auto *button = window->findChild<QPushButton *>("captureCancel")) QTest::mouseClick(button, Qt::LeftButton); return; }
        if (auto *picker = window->findChild<QWidget *>("colorPicker"); picker && picker->isVisible()) { picker->setFocus(); QTest::keyClick(picker, Qt::Key_Return); return; }
        if (allowed) return;
        if (auto *list = window->findChild<QListWidget *>("captureSources"); list && list->isVisible() && list->count() > 0) QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, list->visualItemRect(list->item(0)).center());
        auto *button = window->findChild<QPushButton *>("captureAllow");
        if (button && button->isEnabled()) { allowed = true; QTest::mouseClick(button, Qt::LeftButton); }
    }
}
void install() { auto *timer = new QTimer(qApp); timer->setInterval(50); QObject::connect(timer, &QTimer::timeout, qApp, input); timer->start(); }
Q_COREAPP_STARTUP_FUNCTION(install)
}
