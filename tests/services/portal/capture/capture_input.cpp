// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <qindaqt/services/compositor_capture/capture_port.h>
#include <QFile>
#include <QDir>
#include <QJsonObject>
#include <QStandardPaths>
#include <QListWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QTimer>
#include <QWindow>
#include <QtGui/qguiapplication_platform.h>
#include <QtTest/QTest>
#include <wayland-client.h>
#include <sys/socket.h>
#include <unistd.h>
namespace {
bool audited = false, allowed = false;
QString testAction, testAudit; qint64 expectedPeer = 0;
bool ordinaryObserved = false, multipleSelection = false;
void auditOrdinary(wl_display *display) {
    // An extra registry on Qt's actual ordinary connection observes its
    // announcements without replacing Qt listeners or owning its display.
    static const wl_registry_listener events{
        [](void *, wl_registry *, uint32_t, const char *interface, uint32_t) {
            QFile file(testAudit); if (file.open(QIODevice::WriteOnly | QIODevice::Append))
                file.write(QByteArray::number(getpid()) + " ordinary fd4 global " + interface + '\n');
        }, [](void *, wl_registry *, uint32_t) {}};
    auto *registry = wl_display_get_registry(display); wl_registry_add_listener(registry, &events, nullptr);
    auto *barrier = wl_display_sync(display);
    static const wl_callback_listener completed{[](void *, wl_callback *callback, uint32_t) {
        wl_callback_destroy(callback); QFile file(testAudit);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append))
            file.write(QByteArray::number(getpid()) + " ordinary fd4 globals complete\n");
    }};
    wl_callback_add_listener(barrier, &completed, nullptr); wl_display_flush(display);
}
class FailureObserver final : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject *receiver, QEvent *) override {
        // AGENT-NOTE: The port is a stack QObject. Public application event
        // filtering observes its events without a production test hook or
        // private GUI access; only its existing public failure signal is read.
        auto *port = qobject_cast<QindaQt::CompositorCapture::CapturePort *>(receiver);
        if (port && !connected) {
            connected = true;
            QObject::connect(port, &QindaQt::CompositorCapture::CapturePort::finished, this,
                [](const QindaQt::CompositorCapture::DecodedCapture &capture) {
                    if (capture.ok()) return;
                    QFile audit(testAudit);
                    if (audit.open(QIODevice::WriteOnly | QIODevice::Append))
                        audit.write("public capture failure: " + capture.error.toUtf8().left(1024) + "\n");
                });
        }
        return false;
    }
private: bool connected = false;
};
void input() {
    const auto action = testAction;
    for (auto *window : QApplication::topLevelWidgets()) {
        if (window->objectName() != "nativeCaptureDialog" || !window->isVisible() || !window->windowHandle() || !window->windowHandle()->isExposed()) continue;
        if (!audited) {
            auto *native = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>(); ucred peer{}; socklen_t size = sizeof(peer);
            if (!native || getsockopt(wl_display_get_fd(native->display()), SOL_SOCKET, SO_PEERCRED, &peer, &size) != 0 || peer.pid != expectedPeer) { QCoreApplication::exit(3); return; }
            QFile audit(testAudit); if (!audit.open(QIODevice::WriteOnly | QIODevice::Append)) { QCoreApplication::exit(4); return; }
            audit.write(QByteArray::number(getpid()) + " ordinary exact-peer mapped\n"); audited = true;
            if (!ordinaryObserved) { ordinaryObserved = true; auditOrdinary(native->display()); }
        }
        if (action == "hold") return;
        if (action == "cancel") { if (auto *button = window->findChild<QPushButton *>("captureCancel")) QTest::mouseClick(button, Qt::LeftButton); return; }
        if (auto *picker = window->findChild<QWidget *>("colorPicker"); picker && picker->isVisible()) { picker->setFocus(); QTest::keyClick(picker, Qt::Key_Return); return; }
        if (allowed) return;
        if (auto *list = window->findChild<QListWidget *>("captureSources"); list && list->isVisible() && list->count() > 0) {
            if (action == "allow-restore") {
                if (list->selectedItems().isEmpty()) return;
            } else {
                QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, list->visualItemRect(list->item(0)).center());
                if (multipleSelection) for (int index = 1; index < list->count(); ++index)
                    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::ControlModifier, list->visualItemRect(list->item(index)).center());
            }
            if (action == "allow-remember") {
                auto *remember = window->findChild<QCheckBox *>("captureRemember");
                if (!remember || !remember->isVisible()) return;
                if (!remember->isChecked()) QTest::mouseClick(remember, Qt::LeftButton);
                if (!remember->isChecked()) return;
            }
            if (action == "allow-remember" || action == "allow-restore") {
                QFile record(testAudit); if (record.open(QIODevice::WriteOnly | QIODevice::Append)) {
                    record.write(action == "allow-remember" ? "remember=true\n" : "restored=true\n");
                    for (const auto *item : list->selectedItems()) record.write("selected source=" + item->text().toUtf8() + '\n');
                }
            }
        }
        auto *button = window->findChild<QPushButton *>("captureAllow");
        if (button && button->isEnabled()) { allowed = true; QTest::mouseClick(button, Qt::LeftButton); }
    }
}
void install() { auto *app = QCoreApplication::instance(); app->installEventFilter(new FailureObserver(app)); auto *timer = new QTimer(qApp); timer->setInterval(50); QObject::connect(timer, &QTimer::timeout, qApp, input); timer->start(); }
Q_COREAPP_STARTUP_FUNCTION(install)
}

namespace QindaQt::Services::Portal {
bool captureTestFrame(QJsonObject &frame, qint64 compositorPid) {
    testAction = frame.take("test_action").toString(); testAudit = frame.take("test_audit").toString();
    expectedPeer = compositorPid; multipleSelection = frame.value("multiple").toBool();
    return expectedPeer > 0 && (testAction == "allow" || testAction == "allow-remember" || testAction == "allow-restore" || testAction == "cancel" || testAction == "hold")
        && testAudit == QDir(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)).filePath("qindaqt-capture.audit");
}
}
