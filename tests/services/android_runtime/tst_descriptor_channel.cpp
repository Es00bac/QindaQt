// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime_fixtures.h"
#include "qindaqt/services/android_runtime/descriptor_runtime_port.h"
#include "qindaqt/services/android_runtime/producer_endpoint.h"
#include "qindaqt/services/android_runtime/descriptors.h"
#include <QEventLoop>
#include <QTimer>
#include <QTest>
#include <QSignalSpy>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace RuntimeFixtures;
namespace {
// Own only the exact forked inert test child. Never targets an ambient process,
// guest, package helper or user application. Fixtures make no Android OS claim.
struct Child {
    pid_t pid=-1;
    ~Child() { if (pid>0) { int status=0; if (::waitpid(pid,&status,WNOHANG)==0) { ::kill(pid,SIGTERM); ::waitpid(pid,&status,0); } } }
};
void fixtureChildSignals() {
    // Only deliberate fixture retirement uses SIGTERM. Do not inherit QtTest's
    // crash reporter in the fork, which mislabels this expected cleanup.
    struct sigaction action{}; action.sa_handler=SIG_DFL; ::sigemptyset(&action.sa_mask);
    static_cast<void>(::sigaction(SIGTERM,&action,nullptr));
}
int executable() { return ::open("/proc/self/exe",O_PATH|O_CLOEXEC); }
void waitChild() { char byte=0; static_cast<void>(::read(STDIN_FILENO,&byte,0)); ::pause(); }
void sendSnapshot(int fd,const RuntimeSnapshot &value,const QByteArray &secret=key()) {
    const auto packet=encodePacket({PacketKind::Snapshot,secret,value});
    static_cast<void>(::send(fd,packet.constData(),static_cast<size_t>(packet.size()),MSG_NOSIGNAL));
}
}
class DescriptorChannelTest final:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void realForkCredentialsAndNoReplay();
    void sameUidWrongSenderCannotPublish();
    void deadProducerGetterWithdrawsBeforeQueuedNotification();
    void changedExecutableWithdrawsWhilePidLives();
    void wrongKeyAndStaleRevisionRefuse();
    void wrongLifetimeObjectIsUnavailable();
};
void DescriptorChannelTest::realForkCredentialsAndNoReplay() {
    auto pair=createPrivateProducerDescriptors(); QVERIFY(pair[0]>=0);
    const int parentPid=static_cast<int>(::getpid()); const unsigned uid=::getuid();
    const int parentLifetime=holdProducerLifetime(parentPid); const int expectedExecutable=executable();
    QVERIFY(parentLifetime>=0 && expectedExecutable>=0);
    Child child; child.pid=::fork(); QVERIFY(child.pid>=0);
    if (child.pid==0) { fixtureChildSignals();
        ::close(pair[0]);
        ProducerEndpoint endpoint(pair[1],parentLifetime,expectedExecutable,parentPid,uid,key());
        QEventLoop loop; int dispatches=0;
        QObject::connect(&endpoint,&ProducerEndpoint::launchRequested,&loop,[&](const LaunchRequest &operation) {
            ++dispatches;
            static_cast<void>(endpoint.publishResult({operation.operationId,operation.attachmentId,operation.application,
                LaunchPhase::Completed,QStringLiteral("dispatches=%1").arg(dispatches)}));
        });
        QObject::connect(&endpoint,&ProducerEndpoint::disconnected,&loop,&QEventLoop::quit);
        if (!endpoint.live() || !endpoint.publishSnapshot(snapshot())) ::_exit(3);
        QTimer::singleShot(2000,&loop,&QEventLoop::quit); loop.exec(); ::_exit(0);
    }
    ::close(pair[1]); ::close(parentLifetime);
    DescriptorRuntimePort port(pair[0],holdProducerLifetime(child.pid),expectedExecutable,child.pid,uid,key());
    QVERIFY(port.producerLive());
    QTRY_COMPARE_WITH_TIMEOUT(port.snapshot().registrations.size(),1,700);
    QVector<LaunchResult> results;
    QObject::connect(&port,&RuntimePort::launchFinished,&port,[&](const LaunchResult &result){ results.append(result); });
    const auto operation=request(port.snapshot());
    QVERIFY(port.requestLaunch(operation).accepted);
    QTRY_VERIFY_WITH_TIMEOUT(!results.isEmpty(),700);
    QCOMPARE(results.last().message,QStringLiteral("dispatches=1"));
    QVERIFY(port.requestLaunch(operation).accepted); // retained result, no send
    QCOMPARE(results.last().message,QStringLiteral("dispatches=1"));
    auto changed=operation; changed.application.registrationRevision=2;
    QVERIFY(!port.requestLaunch(changed).accepted);
    QCOMPARE(port.snapshot().applications.first().state,ApplicationState::Ready);
}
void DescriptorChannelTest::sameUidWrongSenderCannotPublish() {
    auto pair=createPrivateProducerDescriptors(); QVERIFY(pair[0]>=0);
    Child trusted; trusted.pid=::fork(); QVERIFY(trusted.pid>=0);
    if (trusted.pid==0) { fixtureChildSignals(); ::close(pair[0]); waitChild(); ::_exit(0); }
    DescriptorRuntimePort port(pair[0],holdProducerLifetime(trusted.pid),executable(),trusted.pid,::getuid(),key());
    QVERIFY(port.producerLive());
    Child attacker; attacker.pid=::fork(); QVERIFY(attacker.pid>=0);
    if (attacker.pid==0) { fixtureChildSignals(); sendSnapshot(pair[1],snapshot()); waitChild(); ::_exit(0); }
    ::close(pair[1]);
    QTRY_VERIFY_WITH_TIMEOUT(!port.producerLive(),700);
    QVERIFY(port.snapshot().registrations.isEmpty());
}
void DescriptorChannelTest::deadProducerGetterWithdrawsBeforeQueuedNotification() {
    auto pair=createPrivateProducerDescriptors(); QVERIFY(pair[0]>=0);
    Child child; child.pid=::fork(); QVERIFY(child.pid>=0);
    if (child.pid==0) { fixtureChildSignals(); ::close(pair[0]); sendSnapshot(pair[1],snapshot()); waitChild(); ::_exit(0); }
    ::close(pair[1]);
    DescriptorRuntimePort port(pair[0],holdProducerLifetime(child.pid),executable(),child.pid,::getuid(),key());
    QTRY_COMPARE_WITH_TIMEOUT(port.snapshot().registrations.size(),1,700);
    QVERIFY(port.requestLaunch(request(port.snapshot())).accepted);
    ::kill(child.pid,SIGTERM); int status=0; QVERIFY(::waitpid(child.pid,&status,0)==child.pid); child.pid=-1;
    // No processEvents here: the pidfd getter itself must fence cached truth.
    QVERIFY(!port.producerLive()); QVERIFY(port.snapshot().registrations.isEmpty());
    QVERIFY(!port.requestLaunch(request(snapshot())).accepted);
}
void DescriptorChannelTest::changedExecutableWithdrawsWhilePidLives() {
    auto pair=createPrivateProducerDescriptors(); std::array<int,2> release{-1,-1};
    QVERIFY(pair[0]>=0 && ::pipe2(release.data(),O_CLOEXEC)==0);
    Child child; child.pid=::fork(); QVERIFY(child.pid>=0);
    if (child.pid==0) { fixtureChildSignals();
        ::close(pair[0]); ::close(release[1]); sendSnapshot(pair[1],snapshot());
        char byte=0; static_cast<void>(::read(release[0],&byte,1));
        ::fcntl(pair[1],F_SETFD,0); // malicious exec retains the endpoint
        ::execl("/bin/sleep","sleep","0.5",static_cast<char *>(nullptr)); ::_exit(4);
    }
    ::close(pair[1]); ::close(release[0]);
    DescriptorRuntimePort port(pair[0],holdProducerLifetime(child.pid),executable(),child.pid,::getuid(),key());
    QTRY_COMPARE_WITH_TIMEOUT(port.snapshot().registrations.size(),1,700);
    QVERIFY(::write(release[1],"x",1)==1); ::close(release[1]);
    QTRY_VERIFY_WITH_TIMEOUT(!port.producerLive(),700);
    QVERIFY(port.snapshot().registrations.isEmpty());
}
void DescriptorChannelTest::wrongKeyAndStaleRevisionRefuse() {
    for (const bool stale:{false,true}) {
        auto pair=createPrivateProducerDescriptors(); Child child; child.pid=::fork(); QVERIFY(child.pid>=0);
        if (child.pid==0) { fixtureChildSignals();
            ::close(pair[0]); auto value=snapshot();
            if (stale) { value.revision=2; sendSnapshot(pair[1],value); value.revision=1; }
            sendSnapshot(pair[1],value,stale?key():QByteArray(32,'x')); waitChild(); ::_exit(0);
        }
        ::close(pair[1]);
        DescriptorRuntimePort port(pair[0],holdProducerLifetime(child.pid),executable(),child.pid,::getuid(),key());
        QTRY_VERIFY_WITH_TIMEOUT(!port.producerLive(),700);
        QVERIFY(port.snapshot().registrations.isEmpty());
    }
}
void DescriptorChannelTest::wrongLifetimeObjectIsUnavailable() {
    auto pair=createPrivateProducerDescriptors();
    const int wrong=::open("/dev/null",O_RDONLY|O_CLOEXEC); QVERIFY(wrong>=0);
    DescriptorRuntimePort port(pair[0],wrong,executable(),static_cast<int>(::getpid()),::getuid(),key());
    QVERIFY(!port.producerLive()); ::close(pair[1]);
}
QTEST_GUILESS_MAIN(DescriptorChannelTest)
#include "tst_descriptor_channel.moc"
