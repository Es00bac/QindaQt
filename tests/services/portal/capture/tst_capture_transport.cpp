// SPDX-License-Identifier: GPL-3.0-or-later
#include "../foundation/support/private_bus.h"
#include <qindaqt/services/compositor_capture/kwin_capture_port.h>
#include <qindaqt/compositor_names/compositor_names.h>
#include <QDBusVirtualObject>
#include <QDBusUnixFileDescriptor>
#include <QThread>
#include <QtTest>
#include <cerrno>
#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <unistd.h>
using namespace QindaQt::CompositorCapture;
namespace {
class Writer final : public QObject {
public:
    Writer(int ownedFd, QDBusConnection bus, QDBusMessage reply, int delay, bool early, QObject *parent)
        : QObject(parent), fd(ownedFd), connection(bus), response(reply), bytes(4096, '\0'), replied(early) {
        const int flags = fcntl(fd, F_GETFL); if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        response.setArguments({QVariantMap{{"type", "raw"}, {"width", 64U}, {"height", 16U}, {"stride", 256U}, {"format", uint(QImage::Format_RGB32)}}});
        if (replied) connection.send(response);
        timer.setInterval(delay); connect(&timer, &QTimer::timeout, this, [this] { advance(); }); timer.start();
    }
    ~Writer() override { if (fd >= 0) ::close(fd); }
    void advance() {
        if (fd < 0) return;
        const auto count = ::write(fd, bytes.constData(), static_cast<size_t>(qMin(bytes.size(), qsizetype(512))));
        if (count > 0) bytes.remove(0, count);
        else if (count < 0 && (errno == EAGAIN || errno == EINTR)) return;
        if (bytes.isEmpty() || count < 0) {
            timer.stop(); ::close(fd); fd = -1;
            if (!replied) connection.send(response); deleteLater();
        }
    }
private:
    int fd; QDBusConnection connection; QDBusMessage response; QByteArray bytes; QTimer timer; bool replied;
};
class CaptureEndpoint final : public QDBusVirtualObject {
public:
    explicit CaptureEndpoint(QObject *parent) : QDBusVirtualObject(parent) {}
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &call, const QDBusConnection &bus) override {
        if (call.interface() != QString(QindaQt::CompositorNames::screenshotInterface) || call.signature() != "a{sv}h") return false;
        const auto arguments = call.arguments(); const auto descriptor = qvariant_cast<QDBusUnixFileDescriptor>(arguments.last());
        if (!descriptor.isValid()) return false;
        call.setDelayedReply(true); const int fd = dup(descriptor.fileDescriptor()); if (fd < 0) return false;
        new Writer(fd, bus, call.createReply(), delay, earlyReply, this); ++calls; return true;
    }
    int delay = 15, calls = 0; bool earlyReply = false;
};
}
class CaptureTransportTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init() {
        fixture = std::make_unique<PortalPrivateBus>(); QVERIFY(fixture->start()); compositor = fixture->connect(); client = fixture->connect();
        QVERIFY(compositor->registerService(QString(QindaQt::CompositorNames::service)));
        QVERIFY(compositor->registerService(QString(QindaQt::CompositorNames::screenshotService)));
        endpoint = std::make_unique<CaptureEndpoint>(nullptr);
        QVERIFY(compositor->registerVirtualObject(QString(QindaQt::CompositorNames::screenshotPath), endpoint.get()));
        port = std::make_unique<KWinCapturePort>(*client);
        connect(port.get(), &CapturePort::finished, this, [this](const DecodedCapture &value) { results.append(value); }); results.clear();
    }
    void cleanup() {
        port.reset(); compositor->unregisterObject(QString(QindaQt::CompositorNames::screenshotPath)); endpoint.reset();
        client.reset(); compositor.reset(); fixture.reset();
    }
    void continuousDrainCanFinishBeforeMetadataAndEofDeadline() {
        auto request = kwinCallFor({}); request.timeoutMilliseconds = 2000; request.pipeGraceMilliseconds = 0;
        QVERIFY(port->capture(request)); QTRY_COMPARE(endpoint->calls, 1); QTRY_COMPARE(results.size(), 1);
        QVERIFY2(results.first().ok(), qPrintable(results.first().error)); QCOMPARE(results.first().image.size(), QSize(64, 16)); QVERIFY(!port->busy());
    }
    void zeroGraceAndBlockedEventLoopNeverPublishLateSuccess() {
        endpoint->delay = 40; endpoint->earlyReply = true; auto request = kwinCallFor({}); request.timeoutMilliseconds = 100; request.pipeGraceMilliseconds = 0;
        QVERIFY(port->capture(request)); QTRY_COMPARE(endpoint->calls, 1);
        QThread::msleep(180); QTRY_COMPARE(results.size(), 1); QVERIFY(!results.first().ok()); QVERIFY(results.first().image.isNull());
        QTest::qWait(400); QCOMPARE(results.size(), 1); QVERIFY(!port->busy());
    }
    void cancelSuppressesPendingPipeAndLateReply() {
        endpoint->delay = 40; auto request = kwinCallFor({}); request.timeoutMilliseconds = 2000; request.pipeGraceMilliseconds = 0;
        QVERIFY(port->capture(request)); QTRY_COMPARE(endpoint->calls, 1); port->cancel(); QVERIFY(!port->busy());
        QTest::qWait(400); QVERIFY(results.isEmpty());
    }
    void nativeOwnerReplacementCannotPublishOldCompatibilityPixels() {
        endpoint->delay = 40; auto request = kwinCallFor({}); request.timeoutMilliseconds = 2000; request.pipeGraceMilliseconds = 0;
        QVERIFY(port->capture(request)); QTRY_COMPARE(endpoint->calls, 1);
        auto replacement = fixture->connect(); QVERIFY(compositor->unregisterService(QString(QindaQt::CompositorNames::service)));
        QVERIFY(replacement->registerService(QString(QindaQt::CompositorNames::service)));
        // Keep the old Screenshot2 owner and writer alive. Only the actual
        // native owner changed; success must still be withdrawn at decoding.
        QTRY_COMPARE(results.size(), 1); QVERIFY(!results.first().ok()); QVERIFY(results.first().image.isNull());
    }
    void invalidBudgetsAndLegacySelectionCompatibility() {
        auto request = kwinCallFor({}); request.timeoutMilliseconds = 0; QVERIFY(!port->capture(request));
        request.timeoutMilliseconds = 100; request.pipeGraceMilliseconds = -1; QVERIFY(!port->capture(request));
        CaptureOptions options; options.mode = CaptureMode::WindowUnderPointer;
        const auto interactive = kwinCallFor(options); QCOMPARE(interactive.timeoutMilliseconds, 120000); QCOMPARE(interactive.pipeGraceMilliseconds, 10000);
        QCOMPARE(interactive.method, QStringLiteral("CaptureInteractive")); QCOMPARE(endpoint->calls, 0);
    }
private:
    std::unique_ptr<PortalPrivateBus> fixture; std::unique_ptr<QDBusConnection> compositor, client;
    std::unique_ptr<CaptureEndpoint> endpoint; std::unique_ptr<KWinCapturePort> port; QList<DecodedCapture> results;
};
int main(int argc, char **argv) {
    rlimit cores{0, 0}; if (setrlimit(RLIMIT_CORE, &cores)) return 2; signal(SIGPIPE, SIG_IGN);
    qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent-qindaqt-capture-transport-bus");
    qputenv("DBUS_SYSTEM_BUS_ADDRESS", "unix:path=/nonexistent-qindaqt-capture-transport-system-bus");
    QCoreApplication app(argc, argv); CaptureTransportTest test; return QTest::qExec(&test, argc, argv);
}
#include "tst_capture_transport.moc"
