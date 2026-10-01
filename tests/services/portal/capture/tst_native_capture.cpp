// SPDX-License-Identifier: GPL-3.0-or-later
#include "pipewire_frames.h"
#include <qindaqt/services/portal/capture_types.h>
#include <qindaqt/compositor_names/compositor_names.h>
#include <qindaqt/platform/compositor_attachment/compositor_attachment.h>
#include <QProcessEnvironment>
#include <QList>
#include <QSocketNotifier>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QFile>
#include <QDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <cerrno>
#include <QElapsedTimer>
#include <QWindow>
#include <utility>
#include <QPainter>
#include <QProcess>
#include <QUrl>
#include <QUuid>
#include <QWidget>
#include <QtTest>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>
using namespace QindaQt::Services::Portal;
class Responses final : public QObject {
    Q_OBJECT
public: int count = 0; quint32 response = 99; QVariantMap results;
public Q_SLOTS: void receive(quint32 r, const QVariantMap &v) { ++count; response = r; results = v; }
};
class ExportProcess final : public QProcess {
public: ~ExportProcess() override { if (state() != NotRunning) { kill(); waitForFinished(3000); } }
};
class NativeCaptureTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void initTestCase() {
        QCOMPARE(prctl(PR_GET_DUMPABLE), 1); registerCaptureWireTypes(); QVERIFY(bus.isConnected()); QTRY_VERIFY(bus.interface()->serviceOwner(QString(QindaQt::CompositorNames::service)).isValid());
        QVERIFY(bus.registerService("org.freedesktop.portal.Documents")); QVERIFY(bus.registerService("org.freedesktop.impl.portal.PermissionStore")); QVERIFY(bus.registerService("org.qindaqt.Power1"));
        selected = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"), "capture-supervisor"));
        QTRY_VERIFY_WITH_TIMEOUT(bus.interface()->serviceOwner("org.freedesktop.impl.portal.desktop.qindaqt.capture").isValid(), 10000);
        const auto brokerOwner = bus.interface()->serviceOwner("org.freedesktop.impl.portal.desktop.qindaqt.capture").value();
        const auto brokerPid = bus.interface()->servicePid(brokerOwner); QVERIFY(brokerPid.isValid()); QVERIFY(brokerPid.value() > 0);
        char executable[4096]; errno = 0;
        const auto proc = QByteArray("/proc/")+QByteArray::number(brokerPid.value())+"/exe";
        const auto executableResult = readlink(proc.constData(), executable, sizeof(executable)); const int executableError = errno;
        QCOMPARE(executableResult, ssize_t(-1)); QCOMPARE(executableError, EACCES);
        qputenv("QINDAQT_CAPTURE_TEST_AUDIT", QFile::encodeName(QDir(qEnvironmentVariable("XDG_RUNTIME_DIR")).filePath("qindaqt-capture.audit")));
        QindaQt::Platform::Compositor::CompositorAttachment attachment(*selected, qEnvironmentVariable("XDG_RUNTIME_DIR"), [this](const QString &owner) { return owner == selected->baseService(); });
        QVERIFY(attachment.attach(selected->baseService(), "qindaqt-8")); const int pixelFd = attachment.openConnection(); QVERIFY(pixelFd >= 0);
        auto pixelEnv = QProcessEnvironment::systemEnvironment(); pixelEnv.remove("WAYLAND_DISPLAY"); pixelEnv.insert("WAYLAND_SOCKET", QString::number(pixelFd)); pixels.setProcessEnvironment(pixelEnv);
        pixels.setChildProcessModifier([pixelFd] { if (fcntl(pixelFd, F_SETFD, 0) < 0) _exit(2); }); pixels.start(qEnvironmentVariable("QINDAQT_CAPTURE_TEST_PIXELS")); const bool pixelStarted = pixels.waitForStarted(); ::close(pixelFd); QVERIFY(pixelStarted);
        QByteArray pixelReady; QTRY_VERIFY_WITH_TIMEOUT((pixelReady += pixels.readAllStandardOutput()).contains("mapped private pixels"), 10000); startFrontend();
        QVERIFY(bus.connect("org.freedesktop.portal.Desktop", {}, "org.freedesktop.portal.Request", "Response", &responses, SLOT(receive(quint32,QVariantMap))));
    }
    void init() { reset("allow"); }
    void cleanup() {
        if (QTest::currentTestFailed()) { qInfo().noquote() << frontend.readAllStandardError().right(32768); qInfo().noquote() << "native input/failure audit:" << audit().left(8192); }
        for (const auto &path : std::as_const(requests)) close(path, "Request");
        requests.clear();
    }
    void screenshotActualPixelsColorAndCancel() {
        screenshot("Screenshot"); success();
        const QUrl uri(responses.results.value("uri").toString()); QVERIFY(uri.isLocalFile()); QCOMPARE(uri.toString(QUrl::FullyEncoded), responses.results.value("uri").toString());
        const QImage image(uri.toLocalFile()); QVERIFY(!image.isNull()); QCOMPARE(image.size(), QSize(1100, 820)); QVERIFY(containsFixturePixels(image));
        QFile file(uri.toLocalFile()); QCOMPARE(file.permissions() & (QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ReadOther | QFileDevice::WriteOther), QFileDevice::Permissions{});
        reset("allow"); screenshot("PickColor"); success(); const auto color = qdbus_cast<CaptureColor>(responses.results.value("color")); QVERIFY(color.red >= 0 && color.red <= 1); QVERIFY(color.green >= 0 && color.green <= 1); QVERIFY(color.blue >= 0 && color.blue <= 1);
        QVERIFY((color.red > .7 && color.green < .35) || (color.green > .7 && color.red < .35));
        reset("cancel"); screenshot("Screenshot"); QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 15000); QCOMPARE(responses.response, 1U);
    }
    void closeInvalidParentAndRequesterLoss() {
        screenshot("Screenshot", "wayland:invalid-private-parent"); QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 15000); QCOMPARE(responses.response, 2U);
        reset("hold"); const auto path = screenshot("Screenshot"); mapped(); const auto pid = helperPid(); close(path, "Request"); QTRY_VERIFY(kill(pid, 0) < 0); QCOMPARE(responses.count, 0);
        reset("hold"); auto caller = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"), "capture-requester"));
        request("Screenshot", "Screenshot", {QString{}, QVariantMap{{"interactive", true}}}, *caller); mapped(); const auto orphan = helperPid(); caller.reset(); QDBusConnection::disconnectFromBus("capture-requester"); QTRY_VERIFY(kill(orphan, 0) < 0);
    }
    void validForeignParentGrantAndLossRetireCaptureAndStream() {
        QindaQt::Platform::Compositor::CompositorAttachment attachment(*selected, qEnvironmentVariable("XDG_RUNTIME_DIR"), [this](const QString &owner) { return owner == selected->baseService(); });
        QVERIFY(attachment.attach(selected->baseService(), "qindaqt-8")); const int fd = attachment.openConnection(); QVERIFY(fd >= 0);
        ExportProcess exporter; auto env = QProcessEnvironment::systemEnvironment(); env.remove("WAYLAND_DISPLAY"); env.insert("WAYLAND_SOCKET", QString::number(fd)); exporter.setProcessEnvironment(env);
        exporter.setChildProcessModifier([fd] { if (fcntl(fd, F_SETFD, 0) < 0) _exit(2); }); exporter.start(qEnvironmentVariable("QINDAQT_PORTAL_FOREIGN_EXPORTER")); const bool started = exporter.waitForStarted(); ::close(fd); QVERIFY(started);
        QByteArray output; QTRY_VERIFY_WITH_TIMEOUT((output += exporter.readAllStandardOutput()).contains('\n'), 5000); const auto parent = QString::fromUtf8(output.trimmed());
        screenshot("Screenshot", parent); success(); QString session; createSession(session); select(session); reset("allow"); request("ScreenCast", "Start", {QVariant::fromValue(QDBusObjectPath(session)), parent, QVariantMap{}}); success();
        const auto streams = qdbus_cast<CaptureStreams>(responses.results.value("streams")); QCOMPARE(streams.size(), 1); QVERIFY(streams.first().node > 0); const auto streamPid = helperPid();
        reset("hold"); screenshot("Screenshot", parent); mapped(); const auto pendingPid = helperPid(); exporter.kill(); QVERIFY(exporter.waitForFinished());
        QTRY_VERIFY(kill(streamPid, 0) < 0); QTRY_VERIFY(kill(pendingPid, 0) < 0); QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 15000); QCOMPARE(responses.response, 2U);
    }
    void actualPipeWireNodeFramesSessionCloseAndCancel() {
        QString session; createSession(session); QVERIFY(!session.isEmpty()); select(session); reset("allow");
        request("ScreenCast", "Start", {QVariant::fromValue(QDBusObjectPath(session)), QString{}, QVariantMap{}}); success();
        const auto streams = qdbus_cast<CaptureStreams>(responses.results.value("streams")); QCOMPARE(streams.size(), 1); QVERIFY(streams.first().node > 0);
        auto remote = method("ScreenCast", "OpenPipeWireRemote", {QVariant::fromValue(QDBusObjectPath(session)), QVariantMap{}});
        QDBusPendingCallWatcher opened(bus.asyncCall(remote)); QTRY_VERIFY(opened.isFinished()); const QDBusPendingReply<QDBusUnixFileDescriptor> fd = opened; QVERIFY2(!fd.isError(), qPrintable(fd.error().message())); QVERIFY(fd.value().isValid());
        PipeWireFrames frames(dup(fd.value().fileDescriptor()), streams.first().node); QVERIFY(frames.valid()); QTRY_VERIFY_WITH_TIMEOUT(frames.count() > 3, 15000); QTRY_VERIFY_WITH_TIMEOUT(frames.checksums().size() > 1, 15000); QVERIFY2(frames.error().isEmpty(), qPrintable(frames.error())); QVERIFY(containsFixturePixels(frames.image())); QVERIFY(frames.nodes().contains(streams.first().node));
        const auto pid = helperPid(); close(session, "Session"); QTRY_VERIFY(kill(pid, 0) < 0); QTRY_VERIFY(!frames.nodes().contains(streams.first().node)); QTest::qWait(150); const auto stopped = frames.count(); QTest::qWait(300); QCOMPARE(frames.count(), stopped);
        reset("allow"); QString cancelled; createSession(cancelled); select(cancelled); reset("cancel"); request("ScreenCast", "Start", {QVariant::fromValue(QDBusObjectPath(cancelled)), QString{}, QVariantMap{}}); QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 15000); QCOMPARE(responses.response, 1U);
    }
    void compositorLossWithdrawsStreamsFilesAndPendingPublication() {
        screenshot("Screenshot"); success(); const auto file = QUrl(responses.results.value("uri").toString()).toLocalFile();
        QString session; createSession(session); select(session); reset("allow"); request("ScreenCast", "Start", {QVariant::fromValue(QDBusObjectPath(session)), QString{}, QVariantMap{}}); success();
        const auto streams = qdbus_cast<CaptureStreams>(responses.results.value("streams")); QCOMPARE(streams.size(), 1);
        QDBusPendingCallWatcher opened(bus.asyncCall(method("ScreenCast", "OpenPipeWireRemote", {QVariant::fromValue(QDBusObjectPath(session)), QVariantMap{}}))); QTRY_VERIFY(opened.isFinished()); const QDBusPendingReply<QDBusUnixFileDescriptor> remote = opened; QVERIFY(!remote.isError());
        PipeWireFrames frames(dup(remote.value().fileDescriptor()), streams.first().node); QTRY_VERIFY_WITH_TIMEOUT(frames.count() > 3, 15000); const auto streamPid = helperPid();
        reset("hold"); screenshot("Screenshot"); mapped(); const auto pendingPid = helperPid(); const auto compositorPid = qEnvironmentVariableIntValue("QINDAQT_PORTAL_TEST_COMPOSITOR_PID"); QVERIFY(compositorPid > 1); QCOMPARE(kill(compositorPid, SIGTERM), 0);
        // The PID was created by this private runner and peer-audited by mapped
        // input; native owner loss makes selected admission Unknown/denied.
        QTRY_VERIFY(kill(streamPid, 0) < 0); QTRY_VERIFY(kill(pendingPid, 0) < 0); QTRY_VERIFY(!QFile::exists(file)); QTRY_VERIFY(!frames.nodes().contains(streams.first().node));
        QTest::qWait(150); const auto stopped = frames.count(); QTest::qWait(300); QCOMPARE(frames.count(), stopped);
        reset("allow"); screenshot("Screenshot"); QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 15000); QCOMPARE(responses.response, 2U); QVERIFY(audit().isEmpty());
    }
    void nativeLockStopsActualStreamPendingCaptureAndRetainedFile() {
        screenshot("Screenshot"); success(); const auto file = QUrl(responses.results.value("uri").toString()).toLocalFile(); QVERIFY(QFile::exists(file));
        QString session; createSession(session); select(session); reset("allow"); request("ScreenCast", "Start", {QVariant::fromValue(QDBusObjectPath(session)), QString{}, QVariantMap{}}); success();
        const auto streams = qdbus_cast<CaptureStreams>(responses.results.value("streams")); QCOMPARE(streams.size(), 1);
        QDBusPendingCallWatcher opened(bus.asyncCall(method("ScreenCast", "OpenPipeWireRemote", {QVariant::fromValue(QDBusObjectPath(session)), QVariantMap{}}))); QTRY_VERIFY(opened.isFinished()); const QDBusPendingReply<QDBusUnixFileDescriptor> remote = opened; QVERIFY(!remote.isError());
        PipeWireFrames frames(dup(remote.value().fileDescriptor()), streams.first().node); QTRY_VERIFY_WITH_TIMEOUT(frames.count() > 3, 15000); const auto streamPid = helperPid();
        reset("hold"); screenshot("Screenshot"); mapped(); const auto capturePid = helperPid();
        auto lock = QDBusMessage::createMethodCall(QString(QindaQt::CompositorNames::service), QString(QindaQt::CompositorNames::nativeLockPath), QString(QindaQt::CompositorNames::nativeLockInterface), "RequestLockWithReceipt"); lock << QUuid::createUuid().toString(QUuid::Id128); bus.asyncCall(lock);
        QTRY_VERIFY(kill(streamPid, 0) < 0); QTRY_VERIFY(kill(capturePid, 0) < 0); QTRY_VERIFY(!QFile::exists(file)); QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 15000); QCOMPARE(responses.response, 2U); QTRY_VERIFY(!frames.nodes().contains(streams.first().node));
        QTest::qWait(150); const auto stopped = frames.count(); QTest::qWait(300); QCOMPARE(frames.count(), stopped);
        reset("allow"); screenshot("Screenshot"); QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 15000); QCOMPARE(responses.response, 2U); QVERIFY(audit().isEmpty());
        // Production authorization OFF qualifies native black/locked retirement,
        // not PAM, trusted greeter/unlock or recall of already consumed buffers.
    }
    void frontendAndBrokerLossWithdrawCaptureAndFiles() {
        screenshot("Screenshot"); success(); const auto result = QUrl(responses.results.value("uri").toString()).toLocalFile(); QVERIFY(QFile::exists(result));
        reset("hold"); screenshot("Screenshot"); mapped(); const auto pid = helperPid(); frontend.terminate(); QVERIFY(frontend.waitForFinished(5000)); QTRY_VERIFY(kill(pid, 0) < 0);
        QTRY_VERIFY(!QFile::exists(result)); startFrontend(); reset("allow"); screenshot("Screenshot"); success();
        const auto retained = QUrl(responses.results.value("uri").toString()).toLocalFile(); QVERIFY(QFile::exists(retained));
        reset("hold"); screenshot("Screenshot"); mapped(); const auto next = helperPid();
        const auto owner = bus.interface()->serviceOwner("org.freedesktop.impl.portal.desktop.qindaqt.capture"); QVERIFY(owner.isValid());
        const auto pidOfBroker = bus.interface()->servicePid(owner.value()); QVERIFY(pidOfBroker.isValid()); QVERIFY(pidOfBroker.value() > 0);
        QCOMPARE(kill(static_cast<pid_t>(pidOfBroker.value()), SIGTERM), 0);
        QTRY_VERIFY(kill(next, 0) < 0); QTRY_VERIFY(!QFile::exists(retained));
        QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 15000); QCOMPARE(responses.response, 2U);

    }
    void cleanupTestCase() { frontend.terminate(); frontend.waitForFinished(5000); if (pixels.state() != QProcess::NotRunning) { pixels.kill(); pixels.waitForFinished(3000); } }
private:
    void startFrontend() {
        frontend.start(QString::fromUtf8(QINDAQT_FRONTEND_EXECUTABLE), {"--replace", "--verbose"}); QVERIFY(frontend.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(bus.interface()->isServiceRegistered("org.freedesktop.portal.Desktop").value(), 10000);
        QDBusPendingCallWatcher registering(bus.asyncCall(method("org.freedesktop.host.portal.Registry", "Register", {QStringLiteral("org.test.Capture"), QVariantMap{}}))); QTRY_VERIFY(registering.isFinished()); const QDBusPendingReply<> registered = registering; QVERIFY2(!registered.isError(), qPrintable(registered.error().message()));
    }
    QDBusMessage method(const char *family, const char *member, const QVariantList &args) {
        const auto interface = QByteArray(family).startsWith("org.") ? QString::fromLatin1(family) : "org.freedesktop.portal."+QString::fromLatin1(family);
        auto call = QDBusMessage::createMethodCall("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", interface, member); call.setArguments(args); return call;
    }
    QString request(const char *family, const char *member, const QVariantList &args, const QDBusConnection &caller) {
        QVariantList tagged = args; auto options = tagged.last().toMap();
        options.insert("handle_token", "r" + QUuid::createUuid().toString(QUuid::Id128)); tagged.last() = options;
        QDBusPendingCallWatcher pending(caller.asyncCall(method(family, member, tagged), 20000)); QElapsedTimer clock; clock.start();
        while (!pending.isFinished() && clock.elapsed() < 5000) { QCoreApplication::processEvents(); QTest::qWait(5); }
        const QDBusPendingReply<QDBusObjectPath> result = pending; if (result.isError()) { qInfo().noquote() << result.error().message(); return {}; }
        if (caller.name() == bus.name()) requests.append(result.value().path());
        return result.value().path();
    }
    QString request(const char *family, const char *member, const QVariantList &args) { return request(family, member, args, bus); }
    QString screenshot(const char *member, const QString &parent = {}) { return request("Screenshot", member, {parent, QVariantMap{{"interactive", true}}}); }
    void createSession(QString &session) { reset("allow"); request("ScreenCast", "CreateSession", {QVariantMap{{"session_handle_token", "s" + QUuid::createUuid().toString(QUuid::Id128)}}}); QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 10000); QCOMPARE(responses.response, 0U); const auto handle = responses.results.value("session_handle"); QCOMPARE(handle.metaType(), QMetaType::fromType<QString>()); session = handle.toString(); }
    void select(const QString &session) { reset("allow"); request("ScreenCast", "SelectSources", {QVariant::fromValue(QDBusObjectPath(session)), QVariantMap{{"types", 1U}, {"cursor_mode", 1U}}}); QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 10000); QCOMPARE(responses.response, 0U); }
    void close(const QString &path, const char *family) { auto call = QDBusMessage::createMethodCall("org.freedesktop.portal.Desktop", path, "org.freedesktop.portal."+QString::fromLatin1(family), "Close"); QDBusPendingCallWatcher closing(bus.asyncCall(call)); QTRY_VERIFY(closing.isFinished()); }
    void reset(const char *action) {
        responses.count = 0; responses.response = 99; responses.results.clear(); QFile(qEnvironmentVariable("QINDAQT_CAPTURE_TEST_AUDIT")).remove();
        QSaveFile control(QDir(qEnvironmentVariable("XDG_RUNTIME_DIR")).filePath("qindaqt-capture-input.json")); QVERIFY(control.open(QIODevice::WriteOnly));
        const auto data = QJsonDocument(QJsonObject{{"action", QString::fromLatin1(action)}}).toJson(QJsonDocument::Compact);
        QCOMPARE(control.write(data), data.size()); QVERIFY(control.commit());
    }
    QByteArray audit() const { QFile f(qEnvironmentVariable("QINDAQT_CAPTURE_TEST_AUDIT")); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray{}; }
    void mapped() {
        QTRY_VERIFY_WITH_TIMEOUT(audit().contains("ordinary exact-peer mapped"), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(audit().contains("ordinary fd4 globals complete"), 5000);
        const QByteArrayList forbidden{"org_kde_plasma_window_management", "org_kde_kwin_fake_input", "zkde_screencast_unstable_v1",
            "org_kde_plasma_activation_feedback", "kde_lockscreen_overlay_v1", "ext_session_lock_manager_v1",
            "zwp_input_panel_v1", "zwp_input_method_v1", "zwp_xwayland_keyboard_grab_manager_v1", "xwayland_shell_v1", "wp_security_context_manager_v1"};
        const auto observed = audit();
        for (const auto &name : forbidden) QVERIFY2(!observed.contains("ordinary fd4 global "+name+'\n'), name.constData());
        for (const auto &line : observed.split('\n')) {
            const auto prefix = line.indexOf("capture fd5 global "); if (prefix < 0) continue;
            const auto name = line.mid(prefix+19);
            QVERIFY2(name == "wl_output" || name == "zxdg_output_manager_v1" || name == "zkde_screencast_unstable_v1", name.constData());
        }
        for (const auto &name : {"wl_output", "zxdg_output_manager_v1", "zkde_screencast_unstable_v1"})
            QVERIFY(observed.contains(QByteArray("capture fd5 global ")+name+'\n'));
    }
    void success() { QTRY_COMPARE_WITH_TIMEOUT(responses.count, 1, 15000); QCOMPARE(responses.response, 0U); mapped(); QVERIFY(audit().contains("control CaptureReady")); }
    pid_t helperPid() const {
        for (const auto &line : audit().split('\n')) {
            if (line.contains("ordinary exact-peer mapped")) return static_cast<pid_t>(line.split(' ').value(0).toLongLong());
        }
        return -1;
    }
    static bool containsFixturePixels(const QImage &image) { for (int y = 0; y < image.height(); y += 17) for (int x = 0; x < image.width(); x += 17) { const auto c = image.pixelColor(x, y); if ((c.red() > 180 && c.green() < 80) || (c.green() > 170 && c.red() < 80)) return true; } return false; }
    QDBusConnection bus = QDBusConnection::sessionBus(); QProcess frontend; Responses responses; ExportProcess pixels;
    std::unique_ptr<QDBusConnection> selected; QStringList requests;
};
int main(int argc, char **argv) {
    struct rlimit cores{0, 0}; if (setrlimit(RLIMIT_CORE, &cores) != 0) return 2;
    QCoreApplication application(argc, argv);
    // The caller is an ordinary app; do not weaken resident/helper protection
    // merely to satisfy the frontend's proc-root caller identity lookup.
    NativeCaptureTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_native_capture.moc"
