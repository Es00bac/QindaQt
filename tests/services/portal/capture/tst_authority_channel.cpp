// SPDX-License-Identifier: GPL-3.0-or-later
#include "authority/channel.h"
#include "native_capture_admission.h"
#include <QDBusContext>
#include <QDBusMessage>
#include <qindaqt/compositor_names/compositor_names.h>
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
#include <utility>
#include <vector>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace QindaQt::Services::Portal::CaptureAuthority;
namespace {
// This actual private-bus receipt endpoint exercises the public native lock
// transport; it is neither compositor authority nor a production admission hook.
class NativeReceipt final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.KWin.NativeLock1")
public:
    explicit NativeReceipt(QDBusConnection connection) : bus(std::move(connection)) {}
    void lock() { locked = true; Q_EMIT lockedChanged(true); }
public Q_SLOTS:
    void RequestStateWithReceipt(const QString &nonce) {
        auto receipt = QDBusMessage::createTargetedSignal(message().service(),
            QString(QindaQt::CompositorNames::nativeLockPath),
            QString(QindaQt::CompositorNames::nativeLockInterface), "stateReceipt");
        receipt << nonce << locked << false;
        bus.send(receipt);
    }
Q_SIGNALS:
    void lockedChanged(bool);
    void protectedChanged(bool);
private:
    QDBusConnection bus;
    bool locked = false;
};
struct Fd { int value = -1; ~Fd() { if (value >= 0) ::close(value); } int take() { return std::exchange(value, -1); } };
bool sendPacket(int fd, const Packet &packet, const std::vector<int> &fds = {}) {
    const auto bytes = encode(packet); iovec data{const_cast<char *>(bytes.constData()), static_cast<size_t>(bytes.size())};
    alignas(cmsghdr) std::array<char, CMSG_SPACE(sizeof(int)*2)> control{};
    msghdr message{}; message.msg_iov = &data; message.msg_iovlen = 1;
    if (!fds.empty()) {
        message.msg_control = control.data(); message.msg_controllen = CMSG_SPACE(sizeof(int)*fds.size());
        auto *item = CMSG_FIRSTHDR(&message); item->cmsg_level = SOL_SOCKET; item->cmsg_type = SCM_RIGHTS; item->cmsg_len = CMSG_LEN(sizeof(int)*fds.size());
        memcpy(CMSG_DATA(item), fds.data(), sizeof(int)*fds.size());
    }
    return sendmsg(fd, &message, MSG_NOSIGNAL) == bytes.size();
}
}
class AuthorityChannelTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void initTestCase() {
        QVERIFY(root.isValid()); QFile config(root.filePath("bus.conf")); QVERIFY(config.open(QIODevice::WriteOnly));
        config.write("<busconfig><type>session</type><listen>unix:path="+QFile::encodeName(root.filePath("bus"))+"</listen><auth>EXTERNAL</auth><policy context='default'><allow own='*'/><allow send_destination='*'/><allow receive_sender='*'/></policy></busconfig>"); config.close();
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("HOME", root.path()); environment.insert("XDG_RUNTIME_DIR", root.path());
        environment.insert("DBUS_SESSION_BUS_ADDRESS", "unix:path="+root.filePath("no-ambient-bus"));
        environment.insert("DBUS_SYSTEM_BUS_ADDRESS", "unix:path="+root.filePath("no-system-bus"));
        daemon.setProcessEnvironment(environment); daemon.start(QString::fromUtf8(QINDAQT_CAPTURE_TEST_DBUS), {"--nofork", "--config-file="+config.fileName(), "--print-address=1"}); QVERIFY(daemon.waitForStarted());
        QByteArray output; QTRY_VERIFY_WITH_TIMEOUT((output += daemon.readAllStandardOutput()).contains('\n'), 5000); address = QString::fromUtf8(output.trimmed());
    }
    void init() {
        owner = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(address, "capture-authority-owner"));
        QVERIFY(owner->isConnected()); QVERIFY(owner->registerService(QString(QindaQt::CompositorNames::service)));
    }
    void cleanup() { owner->unregisterObject(QString(QindaQt::CompositorNames::nativeLockPath)); owner->unregisterService(QString(QindaQt::CompositorNames::service)); owner.reset(); QDBusConnection::disconnectFromBus("capture-authority-owner"); QDBusConnection::disconnectFromBus("capture-admission-client"); }
    void cleanupTestCase() { daemon.terminate(); QVERIFY(daemon.waitForFinished(5000)); }
    void admissionTeardownDoesNotReenterDestroyedStorage_data() {
        QTest::addColumn<bool>("denyWhileLive");
        QTest::newRow("admitted-teardown") << false;
        QTest::newRow("live-denial-then-teardown") << true;
    }
    void admissionTeardownDoesNotReenterDestroyedStorage() {
        QFETCH(bool, denyWhileLive);
        NativeReceipt endpoint(*owner);
        QVERIFY(owner->registerObject(QString(QindaQt::CompositorNames::nativeLockPath), &endpoint,
            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
        auto client = QDBusConnection::connectToBus(address, "capture-admission-client");
        QVERIFY(client.isConnected());
        int pair[2]; QCOMPARE(socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair), 0);
        Fd server{pair[0]}, peer{pair[1]};
        bool storageDestroyed = false;
        int losses = 0, callbacksAfterStorage = 0;
        // Match broker member order: jobs unwind before admission, while the
        // QObject subscriber still exists. External counters avoid touching the
        // destroyed storage even if the regression reintroduces a callback.
        struct Storage { bool &destroyed; ~Storage() { destroyed = true; } };
        struct Subscriber final : QObject {
            std::unique_ptr<QindaQt::Services::Portal::NativeCaptureAdmission> admission;
            Storage jobs;
            Subscriber(QDBusConnection bus, const QString &name, int fd, bool &destroyed,
                       int &losses, int &afterStorage)
                : admission(std::make_unique<QindaQt::Services::Portal::NativeCaptureAdmission>(bus, name, fd)), jobs{destroyed} {
                QObject::connect(admission.get(), &QindaQt::Services::Portal::NativeCaptureAdmission::lost,
                    this, [&destroyed, &losses, &afterStorage] {
                        ++losses;
                        if (destroyed) ++afterStorage;
                    });
            }
        };
        auto subscriber = std::make_unique<Subscriber>(client, owner->baseService(), peer.value,
            storageDestroyed, losses, callbacksAfterStorage);
        QSignalSpy ready(subscriber->admission.get(), &QindaQt::Services::Portal::NativeCaptureAdmission::ready);
        QTRY_VERIFY_WITH_TIMEOUT(subscriber->admission->admitted(), 5000);
        QVERIFY(!ready.isEmpty());
        QCOMPARE(losses, 0);
        if (denyWhileLive) {
            endpoint.lock();
            QTRY_VERIFY_WITH_TIMEOUT(losses > 0, 5000);
            QVERIFY(!subscriber->admission->admitted());
            QCOMPARE(callbacksAfterStorage, 0);
        }
        const auto lossesBeforeTeardown = losses;
        subscriber.reset();
        QVERIFY(storageDestroyed);
        QCOMPARE(callbacksAfterStorage, 0);
        QCOMPARE(losses, lossesBeforeTeardown);
        QCoreApplication::processEvents();
        QCOMPARE(losses, lossesBeforeTeardown);
        QDBusConnection::disconnectFromBus("capture-admission-client");
    }
    void consentOrderingAndDuplicateGrant() {
        int pair[2]; QCOMPARE(socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair), 0); Fd server{pair[0]};
        Channel helper(pair[1], Role::Helper, *owner); int received = 0, lost = 0;
        QVERIFY(helper.start([&](ReceivedPacket &&) { ++received; }, [&] { ++lost; }));
        QVERIFY(sendPacket(server.value, {Wire::Message::Hello, 7, 9, {}, 0})); helper.reconcile(); QVERIFY(helper.live()); QCOMPARE(received, 1);
        QVERIFY(helper.send(Wire::Message::ParentReady, 9)); QVERIFY(helper.send(Wire::Message::ConsentGranted, 9));
        QVERIFY(sendPacket(server.value, {Wire::Message::CaptureReady, 7, 9, {}, 0})); helper.reconcile(); QCOMPARE(received, 2); QCOMPARE(lost, 0);
        QVERIFY(sendPacket(server.value, {Wire::Message::CaptureReady, 7, 9, {}, 0})); helper.reconcile(); QVERIFY(!helper.live()); QCOMPARE(lost, 1); QCOMPARE(received, 2);
    }
    void rejectPreConsentAndStaleGeneration_data() {
        QTest::addColumn<bool>("consented"); QTest::addColumn<quint64>("generation"); QTest::addColumn<quint64>("job");
        QTest::newRow("pre-consent") << false << quint64(7) << quint64(9);
        QTest::newRow("stale-generation") << true << quint64(6) << quint64(9);
        QTest::newRow("wrong-job") << true << quint64(7) << quint64(8);
    }
    void rejectPreConsentAndStaleGeneration() {
        QFETCH(bool, consented); QFETCH(quint64, generation); QFETCH(quint64, job);
        int pair[2]; QCOMPARE(socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair), 0); Fd server{pair[0]};
        Channel helper(pair[1], Role::Helper, *owner); int lost = 0;
        QVERIFY(helper.start([](ReceivedPacket &&) {}, [&] { ++lost; }));
        QVERIFY(sendPacket(server.value, {Wire::Message::Hello, 7, 9, {}, 0})); helper.reconcile();
        if (consented) { QVERIFY(helper.send(Wire::Message::ParentReady, 9)); QVERIFY(helper.send(Wire::Message::ConsentGranted, 9)); }
        QVERIFY(sendPacket(server.value, {Wire::Message::CaptureReady, generation, job, {}, 0})); helper.reconcile(); QCOMPARE(lost, 1); QVERIFY(!helper.live());
    }
    void kernelSenderMismatchIsTerminal() {
        int pair[2]; QCOMPARE(socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair), 0); Fd server{pair[0]};
        Channel broker(pair[1], Role::Broker, *owner); int received = 0, lost = 0;
        QVERIFY(broker.start([&](ReceivedPacket &&) { ++received; }, [&] { ++lost; }));
        const auto bytes = encode({Wire::Message::Hello, 7, 0, {}, 0}); const auto child = fork(); QVERIFY(child >= 0);
        if (child == 0) _exit(send(server.value, bytes.constData(), static_cast<size_t>(bytes.size()), MSG_NOSIGNAL) == bytes.size() ? 0 : 2);
        int result = 0; QCOMPARE(waitpid(child, &result, 0), child); QVERIFY(WIFEXITED(result)); QCOMPARE(WEXITSTATUS(result), 0);
        broker.reconcile(); QCOMPARE(received, 0); QCOMPARE(lost, 1); QVERIFY(!broker.live());
    }
    void ownerReplacementDeniesBeforeQueuedEvents() {
        int pair[2]; QCOMPARE(socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair), 0); Fd server{pair[0]};
        Channel broker(pair[1], Role::Broker, *owner); int lost = 0;
        QVERIFY(broker.start([](ReceivedPacket &&) {}, [&] { ++lost; }));
        QVERIFY(sendPacket(server.value, {Wire::Message::Hello, 7, 0, {}, 0})); broker.reconcile(); QVERIFY(broker.live());
        auto replacement = QDBusConnection::connectToBus(address, "capture-authority-replacement");
        QVERIFY(owner->unregisterService(QString(QindaQt::CompositorNames::service))); QVERIFY(replacement.registerService(QString(QindaQt::CompositorNames::service)));
        QVERIFY(!broker.live()); QCOMPARE(lost, 0); broker.reconcile(); QCOMPARE(lost, 1);
        // Read-through denies immediately; explicit reconciliation delivers
        // terminal loss even if no authority packet is waiting.
        QVERIFY(!broker.send(Wire::Message::StartJob, 1, startPayload(Wire::Scope::Screenshot, owner->baseService(), owner->baseService()))); QCOMPARE(lost, 1);
        replacement.unregisterService(QString(QindaQt::CompositorNames::service)); QDBusConnection::disconnectFromBus("capture-authority-replacement");
    }
    void descriptorsCloseUnlessTakenAndJobIdsNeverReplay() {
        int pair[2]; QCOMPARE(socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair), 0); Fd server{pair[0]};
        int pipes[2]; QCOMPARE(pipe2(pipes, O_CLOEXEC), 0); Fd read{pipes[0]}, write{pipes[1]};
        std::vector<int> received; Channel broker(pair[1], Role::Broker, *owner); int lost = 0;
        QVERIFY(broker.start([&](ReceivedPacket &&packet) { if (packet.packet.message == Wire::Message::JobStarted) { received = packet.fds; for (int fd : received) QVERIFY(fcntl(fd, F_GETFD) & FD_CLOEXEC); } }, [&] { ++lost; }));
        QVERIFY(sendPacket(server.value, {Wire::Message::Hello, 7, 0, {}, 0})); broker.reconcile();
        const auto payload = startPayload(Wire::Scope::Screenshot, owner->baseService(), owner->baseService()); QVERIFY(broker.send(Wire::Message::StartJob, 1, payload));
        QVERIFY(sendPacket(server.value, {Wire::Message::JobStarted, 7, 1, {}, 2}, {write.value, read.value})); broker.reconcile(); QCOMPARE(received.size(), size_t(2));
        for (int fd : received) { errno = 0; const auto result = fcntl(fd, F_GETFD); const int error = errno; QCOMPARE(result, -1); QCOMPARE(error, EBADF); }
        QVERIFY(fcntl(write.value, F_GETFD) >= 0); QVERIFY(!broker.send(Wire::Message::StartJob, 1, payload)); QCOMPARE(lost, 1);
    }
    void hupDeniesBeforeObserverDelivery() {
        int pair[2]; QCOMPARE(socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair), 0); Fd server{pair[0]};
        Channel broker(pair[1], Role::Broker, *owner); int lost = 0;
        QVERIFY(broker.start([](ReceivedPacket &&) {}, [&] { ++lost; })); QVERIFY(sendPacket(server.value, {Wire::Message::Hello, 7, 0, {}, 0})); broker.reconcile(); QVERIFY(broker.live());
        ::close(server.take()); QVERIFY(!broker.live()); QCOMPARE(lost, 0); broker.reconcile(); QCOMPARE(lost, 1);
    }
    void ancillaryCountMismatchIsTerminal() {
        int pair[2]; QCOMPARE(socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair), 0); Fd server{pair[0]};
        int pipes[2]; QCOMPARE(pipe2(pipes, O_CLOEXEC), 0); Fd read{pipes[0]}, write{pipes[1]};
        Channel broker(pair[1], Role::Broker, *owner); int received = 0, lost = 0;
        QVERIFY(broker.start([&](ReceivedPacket &&) { ++received; }, [&] { ++lost; }));
        QVERIFY(sendPacket(server.value, {Wire::Message::Hello, 7, 0, {}, 0}, {read.value})); broker.reconcile();
        QCOMPARE(received, 0); QCOMPARE(lost, 1); QVERIFY(!broker.live()); QVERIFY(fcntl(read.value, F_GETFD) >= 0);
    }
private:
    QTemporaryDir root; QProcess daemon; QString address; std::unique_ptr<QDBusConnection> owner;
};
int main(int argc, char **argv) {
    rlimit cores{0, 0}; if (setrlimit(RLIMIT_CORE, &cores)) return 2;
    qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent-qindaqt-capture-unit-bus");
    qputenv("DBUS_SYSTEM_BUS_ADDRESS", "unix:path=/nonexistent-qindaqt-capture-unit-system-bus");
    QCoreApplication app(argc, argv); AuthorityChannelTest test; return QTest::qExec(&test, argc, argv);
}
#include "tst_authority_channel.moc"
