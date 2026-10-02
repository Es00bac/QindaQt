// SPDX-License-Identifier: GPL-3.0-or-later
#include "authority/packet.h"
#include <qindaqt/compositor_names/compositor_names.h>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QtTest>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>
using namespace QindaQt::Services::Portal::CaptureAuthority;
namespace {
constexpr auto backendName = "org.freedesktop.impl.portal.desktop.qindaqt.capture";
constexpr auto backendPath = "/org/freedesktop/portal/desktop";
// Actual bus/peer authentication remains enabled. Only this private native
// endpoint controls when the two public receipt-protocol messages are sent.
class HeldNativeReceipt final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.KWin.NativeLock1")
public:
    explicit HeldNativeReceipt(QDBusConnection connection) : bus(std::move(connection)) {}
    bool pending() const { return !nonce.isEmpty(); }
    QString requester() const { return call.service(); }
    bool sendReceipt(bool locked = false, bool protectedPresentation = false) {
        auto receipt = QDBusMessage::createTargetedSignal(call.service(),
            QString(QindaQt::CompositorNames::nativeLockPath),
            QString(QindaQt::CompositorNames::nativeLockInterface), "stateReceipt");
        receipt << nonce << locked << protectedPresentation;
        return bus.send(receipt);
    }
    bool sendReply() { return bus.send(call.createReply()); }
public Q_SLOTS:
    void RequestStateWithReceipt(const QString &value) {
        call = message(); nonce = value; setDelayedReply(true);
    }
Q_SIGNALS:
    void lockedChanged(bool);
    void protectedChanged(bool);
private:
    QDBusConnection bus;
    QDBusMessage call;
    QString nonce;
};
}
class BrokerStartupTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void initTestCase() {
        QVERIFY(root.isValid()); QFile config(root.filePath("bus.conf")); QVERIFY(config.open(QIODevice::WriteOnly));
        config.write("<busconfig><type>session</type><listen>unix:path=" + QFile::encodeName(root.filePath("bus")) + "</listen><auth>EXTERNAL</auth><policy context='default'><allow own='*'/><allow send_destination='*'/><allow receive_sender='*'/></policy></busconfig>"); config.close();
        environment = QProcessEnvironment::systemEnvironment();
        environment.insert("HOME", root.path()); environment.insert("XDG_RUNTIME_DIR", root.path());
        environment.insert("DBUS_SESSION_BUS_ADDRESS", "unix:path=" + root.filePath("no-ambient-bus"));
        environment.insert("DBUS_SYSTEM_BUS_ADDRESS", "unix:path=" + root.filePath("no-system-bus"));
        environment.insert("QT_FATAL_WARNINGS", "1");
        daemon.setProcessEnvironment(environment); daemon.start(QString::fromUtf8(QINDAQT_CAPTURE_TEST_DBUS), {"--nofork", "--config-file=" + config.fileName(), "--print-address=1"}); QVERIFY(daemon.waitForStarted());
        QByteArray output; QTRY_VERIFY_WITH_TIMEOUT((output += daemon.readAllStandardOutput()).contains('\n'), 5000);
        address = QString::fromUtf8(output.trimmed()); environment.insert("DBUS_SESSION_BUS_ADDRESS", address);
    }
    void init() {
        owner = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(address, "capture-startup-owner"));
        QVERIFY(owner->isConnected()); QVERIFY(owner->registerService(QString(QindaQt::CompositorNames::service)));
        QVERIFY(owner->registerService("org.freedesktop.portal.Desktop"));
        endpoint = std::make_unique<HeldNativeReceipt>(*owner);
        QVERIFY(owner->registerObject(QString(QindaQt::CompositorNames::nativeLockPath), endpoint.get(), QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
        int pair[2]; QCOMPARE(socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, pair), 0);
        serverFd = pair[0]; const int peerFd = pair[1];
        const int credentials = 1;
        QCOMPARE(setsockopt(serverFd, SOL_SOCKET, SO_PASSCRED, &credentials, sizeof(credentials)), 0);
        QCOMPARE(setsockopt(peerFd, SOL_SOCKET, SO_PASSCRED, &credentials, sizeof(credentials)), 0);
        broker.setProcessEnvironment(environment);
        broker.setChildProcessModifier([peerFd] {
            if (dup2(peerFd, Wire::ControlFd) < 0 || fcntl(Wire::ControlFd, F_SETFD, 0) < 0) _exit(2);
            if (peerFd != Wire::ControlFd) ::close(peerFd);
        });
        broker.start(QString::fromUtf8(QINDAQT_CAPTURE_STARTUP_BACKEND)); const bool started = broker.waitForStarted(); ::close(peerFd); QVERIFY(started);
        QVERIFY(sendPacket({Wire::Message::Hello, 17, 0, {}, 0}));
        QTRY_VERIFY_WITH_TIMEOUT(endpoint->pending(), 3000);
        QCOMPARE(broker.state(), QProcess::Running);
    }
    void cleanup() {
        if (broker.state() != QProcess::NotRunning) { broker.terminate(); QVERIFY(broker.waitForFinished(5000)); }
        const auto stderrBytes = broker.readAllStandardError(); QVERIFY2(stderrBytes.isEmpty(), stderrBytes.constData());
        if (serverFd >= 0) { ::close(serverFd); serverFd = -1; }
        owner->unregisterObject(QString(QindaQt::CompositorNames::nativeLockPath));
        owner->unregisterService(QString(QindaQt::CompositorNames::service));
        owner->unregisterService("org.freedesktop.portal.Desktop");
        endpoint.reset(); owner.reset(); QDBusConnection::disconnectFromBus("capture-startup-owner");
        QDBusConnection::disconnectFromBus("capture-startup-replacement");
        packets.clear();
    }
    void cleanupTestCase() { daemon.terminate(); QVERIFY(daemon.waitForFinished(5000)); }
    void bothNativeMessagesPrecedePublication_data() {
        QTest::addColumn<bool>("receiptFirst");
        QTest::newRow("receipt-without-reply") << true;
        QTest::newRow("reply-without-receipt") << false;
    }
    void bothNativeMessagesPrecedePublication() {
        QFETCH(bool, receiptFirst);
        assertUnpublished();
        QVERIFY(receiptFirst ? endpoint->sendReceipt() : endpoint->sendReply());
        // A real Peer.Ping reply from this exact child bounds delivery without
        // treating elapsed time as evidence that admission has initialized.
        roundTrip(); assertUnpublished();
        QVERIFY(receiptFirst ? endpoint->sendReply() : endpoint->sendReceipt());
        verifyReady(); verifyContentDecision(false);
    }
    void initialLockedIsReadyButCannotStartJob_data() {
        QTest::addColumn<bool>("protectedPresentation");
        QTest::newRow("locking") << false;
        QTest::newRow("locked") << true;
    }
    void initialLockedIsReadyButCannotStartJob() {
        QFETCH(bool, protectedPresentation);
        assertUnpublished(); QVERIFY(endpoint->sendReceipt(true, protectedPresentation)); QVERIFY(endpoint->sendReply());
        verifyReady(); verifyContentDecision(true);
    }
    void ownerReplacementInvalidatesPendingInitialization() {
        assertUnpublished(); QVERIFY(endpoint->sendReceipt()); roundTrip(); assertUnpublished();
        auto replacement = QDBusConnection::connectToBus(address, "capture-startup-replacement"); QVERIFY(replacement.isConnected());
        QVERIFY(owner->unregisterService(QString(QindaQt::CompositorNames::service)));
        QVERIFY(replacement.registerService(QString(QindaQt::CompositorNames::service)));
        // The old endpoint may still send its exact nonce/reply; current-owner
        // read-through must reject it before any publication or Ready.
        QVERIFY(endpoint->sendReply());
        QTRY_COMPARE_WITH_TIMEOUT(broker.state(), QProcess::NotRunning, 6000);
        assertUnpublished();
        QVERIFY(replacement.unregisterService(QString(QindaQt::CompositorNames::service)));
    }
private:
    bool sendPacket(const Packet &packet) {
        const auto bytes = encode(packet);
        return ::send(serverFd, bytes.constData(), static_cast<size_t>(bytes.size()), MSG_NOSIGNAL) == bytes.size();
    }
    void collectPackets() {
        char bytes[Wire::HeaderBytes + Wire::MaxPayloadBytes];
        for (;;) {
            iovec data{bytes, sizeof(bytes)};
            alignas(cmsghdr) std::array<char, CMSG_SPACE(sizeof(ucred))> ancillary{};
            msghdr incoming{}; incoming.msg_iov = &data; incoming.msg_iovlen = 1;
            incoming.msg_control = ancillary.data(); incoming.msg_controllen = ancillary.size();
            const auto size = recvmsg(serverFd, &incoming, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
            if (size < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
            if (size == 0) return;
            QVERIFY(size > 0); QVERIFY(!(incoming.msg_flags & (MSG_TRUNC | MSG_CTRUNC)));
            const auto *credentials = CMSG_FIRSTHDR(&incoming); QVERIFY(credentials);
            QCOMPARE(credentials->cmsg_level, SOL_SOCKET); QCOMPARE(credentials->cmsg_type, SCM_CREDENTIALS);
            QCOMPARE(credentials->cmsg_len, CMSG_LEN(sizeof(ucred)));
            ucred peer{}; memcpy(&peer, CMSG_DATA(credentials), sizeof(peer));
            QCOMPARE(peer.uid, geteuid()); QCOMPARE(peer.pid, static_cast<pid_t>(broker.processId()));
            const auto packet = decode(QByteArray(bytes, static_cast<qsizetype>(size))); QVERIFY(packet); packets.append(*packet);
        }
    }
    void assertUnpublished() {
        const QDBusReply<bool> present = owner->interface()->isServiceRegistered(backendName); QVERIFY(present.isValid()); QVERIFY(!present.value());
        collectPackets(); QVERIFY(packets.isEmpty());
    }
    void roundTrip() {
        auto ping = QDBusMessage::createMethodCall(endpoint->requester(), "/", "org.freedesktop.DBus.Peer", "Ping");
        QDBusPendingCallWatcher pending(owner->asyncCall(ping)); QTRY_VERIFY_WITH_TIMEOUT(pending.isFinished(), 1000);
        const QDBusPendingReply<> reply = pending; QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
    }
    void verifyReady() {
        QTRY_VERIFY_WITH_TIMEOUT((collectPackets(), !packets.isEmpty()), 3000);
        QCOMPARE(packets.size(), 1); QCOMPARE(packets.first().message, Wire::Message::Ready);
        QCOMPARE(packets.first().generation, quint64(17)); QCOMPARE(packets.first().job, quint64(0));
        const QDBusReply<QString> name = owner->interface()->serviceOwner(backendName); QVERIFY(name.isValid());
        QCOMPARE(name.value(), endpoint->requester()); QCOMPARE(packets.first().payload, readyPayload(name.value()));
        const QDBusReply<uint> pid = owner->interface()->servicePid(name.value()); QVERIFY(pid.isValid()); QCOMPARE(pid.value(), static_cast<uint>(broker.processId()));
        packets.clear();
    }
    void verifyContentDecision(bool locked) {
        QString actor = owner->baseService().mid(1); actor.replace('.', '_');
        auto call = QDBusMessage::createMethodCall(backendName, backendPath, "org.freedesktop.impl.portal.Screenshot", "Screenshot");
        call << QVariant::fromValue(QDBusObjectPath(QString(backendPath) + "/request/" + actor + "/startup")) << QStringLiteral("org.test.Capture") << QString{} << QVariantMap{};
        QDBusPendingCallWatcher pending(owner->asyncCall(call));
        if (!locked) {
            QTRY_VERIFY_WITH_TIMEOUT((collectPackets(), !packets.isEmpty()), 3000);
            QCOMPARE(packets.size(), 1); QCOMPARE(packets.first().message, Wire::Message::StartJob);
            const auto job = packets.first().job; QVERIFY(job != 0);
            // No helper/pixels are synthesized: the parent refuses this actual
            // admitted request after proving it crossed the StartJob boundary.
            QVERIFY(sendPacket({Wire::Message::Error, 17, job, QByteArray::fromHex("0300"), 0}));
        }
        QTRY_VERIFY_WITH_TIMEOUT(pending.isFinished(), 3000);
        const QDBusPendingReply<quint32, QVariantMap> reply = pending; QVERIFY2(!reply.isError(), qPrintable(reply.error().message())); QCOMPARE(reply.argumentAt<0>(), 2U);
        if (locked) { collectPackets(); QVERIFY(packets.isEmpty()); }
    }
    QTemporaryDir root; QProcess daemon, broker; QProcessEnvironment environment; QString address;
    std::unique_ptr<QDBusConnection> owner; std::unique_ptr<HeldNativeReceipt> endpoint;
    int serverFd = -1; QList<Packet> packets;
};
int main(int argc, char **argv) {
    rlimit cores{0, 0}; if (setrlimit(RLIMIT_CORE, &cores)) return 2;
    qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent-qindaqt-capture-startup-bus");
    qputenv("DBUS_SYSTEM_BUS_ADDRESS", "unix:path=/nonexistent-qindaqt-capture-startup-system-bus");
    QCoreApplication app(argc, argv); BrokerStartupTest test; return QTest::qExec(&test, argc, argv);
}
#include "tst_broker_startup.moc"
