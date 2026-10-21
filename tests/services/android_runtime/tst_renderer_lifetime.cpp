// SPDX-License-Identifier: GPL-3.0-or-later
#include "renderer_fork_fixture.h"
#include "qindaqt/services/android_runtime/renderer_lifetime.h"
#include <QTest>
#include <fcntl.h>
#include <cerrno>
using namespace RendererForkFixture;
namespace { int executable() { return ::open("/proc/self/exe",O_PATH|O_CLOEXEC); } }
class RendererLifetimeTest final:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void rejectsWrongLifetimeAndExecutable();
    void duplicateDescriptorClosesOnce();
    void deathWithdrawsWithoutQueuedNotification();
    void execReplacementWithdrawsWhilePidLives();
    void splitCollectorAndRendererRemainDistinct();
    void collectorLossAndRuntimeReplacementWithdraw();
};
void RendererLifetimeTest::rejectsWrongLifetimeAndExecutable() {
    Child child; startRenderer(child); QVERIFY(child.pid>0);
    HeldRendererLifetime wrongObject(::open("/dev/null",O_RDONLY|O_CLOEXEC),executable(),child.pid);
    QVERIFY(!wrongObject.processId());
    HeldRendererLifetime wrongPid(holdProducerLifetime(static_cast<int>(::getpid())),executable(),child.pid);
    QVERIFY(!wrongPid.processId());
    HeldRendererLifetime wrongExecutable(holdProducerLifetime(child.pid),::open("/bin/sleep",O_PATH|O_CLOEXEC),child.pid);
    QVERIFY(!wrongExecutable.processId());
    HeldRendererLifetime correct(holdProducerLifetime(child.pid),executable(),child.pid);
    QCOMPARE(correct.processId().value_or(0),child.pid);
}
void RendererLifetimeTest::duplicateDescriptorClosesOnce() {
    const int fd=::open("/dev/null",O_RDONLY|O_CLOEXEC); QVERIFY(fd>=0);
    { HeldRendererLifetime duplicate(fd,fd,static_cast<int>(::getpid())); QVERIFY(!duplicate.processId()); }
    errno=0; QCOMPARE(::fcntl(fd,F_GETFD),-1); QCOMPARE(errno,EBADF);
}
void RendererLifetimeTest::deathWithdrawsWithoutQueuedNotification() {
    Child child; startRenderer(child); QVERIFY(child.pid>0);
    HeldRendererLifetime lifetime(holdProducerLifetime(child.pid),executable(),child.pid);
    QCOMPARE(lifetime.processId().value_or(0),child.pid);
    child.retire();
    QVERIFY(!lifetime.processId()); QVERIFY(!lifetime.processId());
}
void RendererLifetimeTest::execReplacementWithdrawsWhilePidLives() {
    std::array<int,2> commands{-1,-1}; QVERIFY(::pipe2(commands.data(),O_CLOEXEC)==0);
    Child child; child.pid=::fork(); QVERIFY(child.pid>=0);
    if (child.pid==0) {
        childSignals(); ::close(commands[1]); char byte=0;
        [[maybe_unused]] const auto ignored=::read(commands[0],&byte,1);
        ::execl("/bin/sleep","sleep","2",static_cast<char *>(nullptr)); ::_exit(4);
    }
    ::close(commands[0]); child.control=commands[1];
    HeldRendererLifetime lifetime(holdProducerLifetime(child.pid),executable(),child.pid);
    QCOMPARE(lifetime.processId().value_or(0),child.pid);
    QVERIFY(::write(child.control,"x",1)==1);
    QTRY_VERIFY_WITH_TIMEOUT(!lifetime.processId(),700);
    int status=0; QCOMPARE(::waitpid(child.pid,&status,WNOHANG),0);
    QVERIFY(!lifetime.processId());
}
void RendererLifetimeTest::splitCollectorAndRendererRemainDistinct() {
    Child renderer; startRenderer(renderer); QVERIFY(renderer.pid>0);
    Child collector; const int channel=startCollector(collector); QVERIFY(channel>=0 && collector.pid>0);
    DescriptorRuntimePort port(channel,holdProducerLifetime(collector.pid),executable(),collector.pid,::getuid(),key());
    HeldRendererLifetime lifetime(holdProducerLifetime(renderer.pid),executable(),renderer.pid);
    RuntimeRendererBinding binding(port,lifetime,snapshot().attachmentId,incarnation);
    QVERIFY(!binding.observe());
    QVERIFY(::write(collector.control,"p",1)==1);
    QTRY_VERIFY_WITH_TIMEOUT(binding.observe().has_value(),700);
    const auto observation=binding.observe(); QVERIFY(observation);
    QCOMPARE(observation->processId,renderer.pid); QVERIFY(observation->processId!=collector.pid);
    QCOMPARE(observation->runtimeAttachmentId,snapshot().attachmentId);
    QCOMPARE(observation->rendererIncarnation,incarnation);
    RuntimeRendererBinding wrongRuntime(port,lifetime,incarnation,incarnation); QVERIFY(!wrongRuntime.observe());
    RuntimeRendererBinding invalidIncarnation(port,lifetime,snapshot().attachmentId,QStringLiteral("title"));
    QVERIFY(!invalidIncarnation.observe());
    renderer.retire(); // no processEvents after held renderer death
    QVERIFY(port.authenticatedPeer()); QVERIFY(!binding.observe());
}
void RendererLifetimeTest::collectorLossAndRuntimeReplacementWithdraw() {
    for (const char command:{'u','r','c','d'}) {
        Child renderer; startRenderer(renderer); QVERIFY(renderer.pid>0);
        Child collector; const int channel=startCollector(collector); QVERIFY(channel>=0 && collector.pid>0);
        DescriptorRuntimePort port(channel,holdProducerLifetime(collector.pid),executable(),collector.pid,::getuid(),key());
        HeldRendererLifetime lifetime(holdProducerLifetime(renderer.pid),executable(),renderer.pid);
        RuntimeRendererBinding binding(port,lifetime,snapshot().attachmentId,incarnation);
        QVERIFY(::write(collector.control,"p",1)==1);
        QTRY_VERIFY_WITH_TIMEOUT(binding.observe().has_value(),700);
        if (command=='d') { collector.retire(); QVERIFY(!binding.observe()); }
        else {
            QVERIFY(::write(collector.control,&command,1)==1);
            if (command=='c') {
                char ack=0; QVERIFY(::read(collector.control,&ack,1)==1); QCOMPARE(ack,'x');
                QVERIFY(!binding.observe()); // no processEvents after acknowledged socket close
            } else { QTRY_VERIFY_WITH_TIMEOUT(!binding.observe(),700); }
        }
        QCOMPARE(lifetime.processId().value_or(0),renderer.pid);
    }
}
QTEST_GUILESS_MAIN(RendererLifetimeTest)
#include "tst_renderer_lifetime.moc"
