// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime_fixtures.h"
#include "qindaqt/compositor/android_renderer_evidence.h"
#include "qindaqt/services/android_runtime/descriptors.h"
#include <QTest>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace RuntimeFixtures;
using namespace QindaQt::Compositor;
namespace {
const QString windowId=QStringLiteral("b1234567-1234-4234-8234-123456789abc");
const QString incarnation=QStringLiteral("c1234567-1234-4234-8234-123456789abc");
// Each fixture owns an inert fork, not a guest or ambient user process.
struct Child {
    pid_t pid=-1;
    int control=-1;
    ~Child() { if (control>=0) ::close(control); retire(); }
    void retire() {
        if (pid<=0) return;
        int status=0;
        if (::waitpid(pid,&status,WNOHANG)==0) {
            ::kill(pid,SIGTERM); static_cast<void>(::waitpid(pid,&status,0));
        }
        pid=-1;
    }
};
void publish(int socket,const RuntimeSnapshot &value) {
    const auto packet=encodePacket({PacketKind::Snapshot,key(),value});
    static_cast<void>(::send(socket,packet.constData(),static_cast<size_t>(packet.size()),MSG_NOSIGNAL));
}
void childLoop(int socket,int control) {
    struct sigaction action{}; action.sa_handler=SIG_DFL; ::sigemptyset(&action.sa_mask);
    static_cast<void>(::sigaction(SIGTERM,&action,nullptr));
    RuntimeSnapshot value=snapshot(); char command=0;
    while (::read(control,&command,1)==1) {
        if (command=='c') {
            ::close(socket); [[maybe_unused]] const auto ignored=::write(control,"x",1); ::pause(); ::_exit(0);
        }
        if (command=='u') { value.state=RuntimeState::Unavailable; ++value.revision; }
        if (command=='r') { value.attachmentId=incarnation; value.revision=1; }
        publish(socket,value);
    }
    ::_exit(0);
}
class FakeSource final:public AndroidRendererEvidenceSource {
public:
    std::optional<AndroidPlatformOrigin> value;
    mutable int reads=0;
    std::optional<AndroidPlatformOrigin> observe(const NativeWindowPeer &) const override {
        ++reads; return value;
    }
};
}
class RendererEvidenceTest final:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void rejectsNonNativeAndUnboundPeers();
    void validatesExactProviderBinding();
    void liveCredentialMatchAndImmediateDeathFence();
    void runtimeWithdrawalAndReplacement();
    void socketLossWithdrawsBeforeQueuedNotification();
};
void RendererEvidenceTest::rejectsNonNativeAndUnboundPeers() {
    FakeSource source;
    source.value=AndroidPlatformOrigin{windowId,snapshot().attachmentId,incarnation,123};
    for (const NativeWindowPeer &peer:{NativeWindowPeer{windowId,false,123},
            NativeWindowPeer{windowId,true,0},NativeWindowPeer{windowId,true,1},
            NativeWindowPeer{QStringLiteral("title or app_id"),true,123}}) {
        QVERIFY(!androidPlatformOrigin(peer,source));
    }
    QCOMPARE(source.reads,0);
}
void RendererEvidenceTest::validatesExactProviderBinding() {
    FakeSource source;
    const NativeWindowPeer peer{windowId,true,123};
    QVERIFY(!androidPlatformOrigin(peer,source));
    source.value=AndroidPlatformOrigin{windowId,snapshot().attachmentId,incarnation,123};
    QVERIFY(androidPlatformOrigin(peer,source));
    source.value->windowId=incarnation; QVERIFY(!androidPlatformOrigin(peer,source));
    source.value->windowId=windowId; source.value->kernelProcessId=124;
    QVERIFY(!androidPlatformOrigin(peer,source));
    source.value->kernelProcessId=123; source.value->runtimeAttachmentId=QStringLiteral("unqualified");
    QVERIFY(!androidPlatformOrigin(peer,source));
    source.value->runtimeAttachmentId=snapshot().attachmentId; source.value->rendererIncarnation={};
    QVERIFY(!androidPlatformOrigin(peer,source));
}
void RendererEvidenceTest::liveCredentialMatchAndImmediateDeathFence() {
    auto pair=createPrivateProducerDescriptors(); std::array<int,2> commands{-1,-1};
    QVERIFY(pair[0]>=0 && ::socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,commands.data())==0);
    Child child; child.pid=::fork(); QVERIFY(child.pid>=0);
    if (child.pid==0) { ::close(pair[0]); ::close(commands[1]); childLoop(pair[1],commands[0]); }
    ::close(pair[1]); ::close(commands[0]); child.control=commands[1];
    DescriptorRuntimePort port(pair[0],holdProducerLifetime(child.pid),::open("/proc/self/exe",O_PATH|O_CLOEXEC),
                               child.pid,::getuid(),key());
    QVERIFY(port.producerLive()); QVERIFY(!port.authenticatedPeer());
    AdmittedRendererDescriptor source(port,snapshot().attachmentId,incarnation);
    const NativeWindowPeer peer{windowId,true,child.pid};
    QVERIFY(!androidPlatformOrigin(peer,source));
    QVERIFY(::write(child.control,"p",1)==1);
    QTRY_VERIFY_WITH_TIMEOUT(port.authenticatedPeer().has_value(),700);
    const auto process=port.authenticatedPeer();
    QCOMPARE(process->processId,child.pid); QCOMPARE(process->userId,::getuid());
    const auto origin=androidPlatformOrigin(peer,source); QVERIFY(origin);
    QCOMPARE(origin->windowId,windowId); QCOMPARE(origin->runtimeAttachmentId,snapshot().attachmentId);
    QCOMPARE(origin->rendererIncarnation,incarnation); QCOMPARE(origin->kernelProcessId,child.pid);
    // A separate host broker's matching snapshot never lends its PID to a window.
    QVERIFY(!androidPlatformOrigin({windowId,true,::getpid()},source));
    QVERIFY(!androidPlatformOrigin({windowId,false,child.pid},source));
    child.retire(); // no Qt notification dispatch after death
    QVERIFY(!port.authenticatedPeer()); QVERIFY(!androidPlatformOrigin(peer,source));
}
void RendererEvidenceTest::runtimeWithdrawalAndReplacement() {
    for (const char command:{'u','r'}) {
        auto pair=createPrivateProducerDescriptors(); std::array<int,2> commands{-1,-1};
        QVERIFY(pair[0]>=0 && ::socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,commands.data())==0);
        Child child; child.pid=::fork(); QVERIFY(child.pid>=0);
        if (child.pid==0) { ::close(pair[0]); ::close(commands[1]); childLoop(pair[1],commands[0]); }
        ::close(pair[1]); ::close(commands[0]); child.control=commands[1];
        DescriptorRuntimePort port(pair[0],holdProducerLifetime(child.pid),::open("/proc/self/exe",O_PATH|O_CLOEXEC),
                                   child.pid,::getuid(),key());
        AdmittedRendererDescriptor source(port,snapshot().attachmentId,incarnation);
        const NativeWindowPeer peer{windowId,true,child.pid};
        QVERIFY(::write(child.control,"p",1)==1);
        QTRY_VERIFY_WITH_TIMEOUT(androidPlatformOrigin(peer,source).has_value(),700);
        QVERIFY(::write(child.control,&command,1)==1);
        QTRY_VERIFY_WITH_TIMEOUT(!androidPlatformOrigin(peer,source),700);
        // Withdrawn runtime truth does not turn the still-live host process into Native.
        QVERIFY(port.producerLive());
    }
}
void RendererEvidenceTest::socketLossWithdrawsBeforeQueuedNotification() {
    auto pair=createPrivateProducerDescriptors(); std::array<int,2> commands{-1,-1};
    QVERIFY(pair[0]>=0 && ::socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,commands.data())==0);
    Child child; child.pid=::fork(); QVERIFY(child.pid>=0);
    if (child.pid==0) { ::close(pair[0]); ::close(commands[1]); childLoop(pair[1],commands[0]); }
    ::close(pair[1]); ::close(commands[0]); child.control=commands[1];
    DescriptorRuntimePort port(pair[0],holdProducerLifetime(child.pid),::open("/proc/self/exe",O_PATH|O_CLOEXEC),
                               child.pid,::getuid(),key());
    AdmittedRendererDescriptor source(port,snapshot().attachmentId,incarnation);
    const NativeWindowPeer peer{windowId,true,child.pid};
    QVERIFY(::write(child.control,"p",1)==1);
    QTRY_VERIFY_WITH_TIMEOUT(androidPlatformOrigin(peer,source).has_value(),700);
    QVERIFY(::write(child.control,"c",1)==1);
    char ack=0; QVERIFY(::read(child.control,&ack,1)==1); QCOMPARE(ack,'x');
    // Peer acknowledged endpoint close and remains alive. No processEvents:
    // withdrawal must come from the getter, not the queued socket notifier.
    QVERIFY(!port.authenticatedPeer()); QVERIFY(!androidPlatformOrigin(peer,source));
    int status=0; QCOMPARE(::waitpid(child.pid,&status,WNOHANG),0);
}
QTEST_GUILESS_MAIN(RendererEvidenceTest)
#include "tst_renderer_evidence.moc"
