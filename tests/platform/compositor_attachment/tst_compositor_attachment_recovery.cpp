// SPDX-License-Identifier: GPL-3.0-or-later
// Event-driven liveness and self-healing re-attachment (ADR-0379). Timers are
// injected; the only real waits are bounded daemon-call timeouts while the
// private daemon is frozen to model the 2026-10-15 stall.
#include "attachment_test_support.h"
#include <QElapsedTimer>
#include <optional>
using namespace AttachmentTestSupport;
using QindaQt::Platform::Recovery::ManualRecoveryTimer;
using QindaQt::Platform::Recovery::RecoveryTimerFactory;
using std::chrono::milliseconds;
namespace {
struct Timers {
  QList<ManualRecoveryTimer *> created;
  RecoveryTimerFactory factory() {
    return [this] {
      auto timer = std::make_unique<ManualRecoveryTimer>();
      created.append(timer.get());
      return timer;
    };
  }
  ManualRecoveryTimer &only() { return *created.first(); }
};
AttachmentRecovery manual(Timers &timers, int callTimeout = 1000) {
  AttachmentRecovery recovery;
  recovery.timers = timers.factory();
  recovery.callTimeoutMilliseconds = callTimeout;
  return recovery;
}
const QString Service = QString(QindaQt::CompositorNames::service);
// One private daemon, one in-process compositor/session pair and a listener
// standing in for the compositor's Wayland socket.
struct Fixture {
  explicit Fixture(int callTimeout = 1000)
      : compositor(bus.connect("compositor")), session(bus.connect("session")),
        client(bus.connect("client")),
        attachment(client, runtime.path(),
                   [this](const QString &owner) {
                     return admitted && owner == session.baseService();
                   },
                   manual(timers, callTimeout)) {
    listener.emplace(runtime.filePath(socketName()));
  }
  bool start(std::optional<PeerExpectation> expected = std::nullopt) {
    return compositor.registerService(Service) &&
           attachment.attach(session.baseService(), socketName(), expected);
  }
  Bus bus;
  QDBusConnection compositor, session, client;
  QTemporaryDir runtime;
  std::optional<Listener> listener;
  Timers timers;
  bool admitted = true;
  CompositorAttachment attachment;
};
} // namespace
class AttachmentRecoveryTest final : public QObject {
  Q_OBJECT
private slots:
  void stalledDaemonNeverDetachesOrBlocksGetters() {
    // A five-second call bound makes any hidden daemon call in a getter show.
    Fixture f(5000);
    QVERIFY(f.start());
    QSignalSpy revoked(&f.attachment, &CompositorAttachment::revoked);
    FrozenDaemon frozen(f.bus);
    QVERIFY(frozen.frozen);
    QElapsedTimer elapsed;
    elapsed.start();
    for (int i = 0; i < 200; ++i) {
      QVERIFY(f.attachment.live());
      QVERIFY(f.attachment.identity());
      QVERIFY(f.attachment.sameBus(f.client));
    }
    for (int i = 0; i < 8; ++i) {
      const int fd = f.attachment.openConnection();
      QVERIFY(fd >= 3);
      close(fd);
    }
    QVERIFY2(elapsed.elapsed() < 2000,
             "attachment getters must not wait for the bus daemon");
    // The superseded design polled the daemon every 100 ms with a 250 ms
    // bound and detached on the first timeout. Nothing may run on a timer now.
    QTest::qWait(400);
    QVERIFY(revoked.isEmpty());
    QVERIFY(f.attachment.live());
    QVERIFY(f.timers.only().requested().isEmpty());
    frozen.thaw();
  }
  void genuineLossDetachesThenOwnerReturnReattaches() {
    Fixture f;
    QSignalSpy revoked(&f.attachment, &CompositorAttachment::revoked);
    QSignalSpy attached(&f.attachment, &CompositorAttachment::attached);
    QVERIFY(f.start());
    QCOMPARE(attached.size(), 1);
    QVERIFY(f.compositor.unregisterService(Service));
    QTRY_COMPARE(revoked.size(), 1);
    QVERIFY(!f.attachment.live());
    QVERIFY(f.attachment.recovering());
    QCOMPARE(f.attachment.openConnection(), -1);
    // Loss arms one bounded retry; the returning owner does not wait for it.
    QCOMPARE(f.timers.only().requested(), QList<milliseconds>{milliseconds{250}});
    QVERIFY(f.compositor.registerService(Service));
    QTRY_COMPARE(attached.size(), 2);
    QVERIFY(f.attachment.live());
    QVERIFY(!f.attachment.recovering());
    QCOMPARE(f.attachment.identity()->compositorOwner, f.compositor.baseService());
    QVERIFY(!f.timers.only().isActive());
    const int fd = f.attachment.openConnection();
    QVERIFY(fd >= 3);
    close(fd);
  }
  void failedReattachmentBacksOffUntilTheDisplayAccepts() {
    Fixture f;
    QSignalSpy attached(&f.attachment, &CompositorAttachment::attached);
    QVERIFY(f.start());
    QVERIFY(f.compositor.unregisterService(Service));
    QTRY_VERIFY(f.attachment.recovering());
    f.listener.reset(); // the compositor's display socket is gone for now
    QVERIFY(f.compositor.registerService(Service));
    auto &timer = f.timers.only();
    QTRY_COMPARE(timer.requested().size(), 2);
    QVERIFY(timer.isActive());
    for (const int expected : {1000, 2000, 4000}) {
      QVERIFY(timer.fire());
      QTRY_VERIFY(timer.isActive());
      QCOMPARE(timer.requested().last(), milliseconds{expected});
      QVERIFY(f.attachment.recovering());
    }
    QCOMPARE(timer.requested().first(), milliseconds{250});
    QCOMPARE(timer.requested().at(1), milliseconds{500});
    QCOMPARE(attached.size(), 1);
    f.listener.emplace(f.runtime.filePath(socketName()));
    QVERIFY(timer.fire());
    QTRY_COMPARE(attached.size(), 2);
    QVERIFY(f.attachment.live());
  }
  void stalledDaemonDuringReattachmentIsRetriedNotRetired() {
    Fixture f(200);
    QSignalSpy attached(&f.attachment, &CompositorAttachment::attached);
    QVERIFY(f.start());
    QVERIFY(f.compositor.unregisterService(Service));
    QTRY_VERIFY(f.attachment.recovering());
    auto &timer = f.timers.only();
    QCOMPARE(timer.requested().size(), 1);
    {
      FrozenDaemon frozen(f.bus);
      QVERIFY(frozen.frozen);
      for (int attempt = 0; attempt < 3; ++attempt) {
        QVERIFY(timer.fire());
        // Each proof call times out against the frozen daemon; the failure
        // re-arms the schedule and keeps the selection.
        QTRY_VERIFY(timer.isActive());
        QVERIFY(f.attachment.recovering());
      }
    }
    QCOMPARE(timer.requested().size(), 4);
    QCOMPARE(timer.requested().last(), milliseconds{2000});
    QVERIFY(f.compositor.registerService(Service));
    QTRY_COMPARE(attached.size(), 2);
    QVERIFY(f.attachment.live());
  }
  void expectedPeerIsEnforcedOnReattachment() {
    Fixture f;
    QSignalSpy attached(&f.attachment, &CompositorAttachment::attached);
    QVERIFY(f.start(PeerExpectation{f.compositor.baseService(),
                                    quint64(getpid())}));
    QVERIFY(f.compositor.unregisterService(Service));
    QTRY_VERIFY(f.attachment.recovering());
    auto replacement = f.bus.connect("replacement");
    QVERIFY(replacement.registerService(Service));
    // Same PID and socket, but not the independently expected unique owner.
    QTRY_VERIFY(!f.attachment.recovering());
    QCOMPARE(attached.size(), 1);
    QVERIFY(!f.attachment.live());
  }
  void sessionOwnerNameReleasePublishesAdmissionWithdrawal() {
    // Shell and clipboard admission follows the current Session1 owner. The
    // superseded design found a withdrawal with its 100 ms poll; now the
    // selected owner giving up a name re-checks admission by event.
    Fixture f;
    QSignalSpy revoked(&f.attachment, &CompositorAttachment::revoked);
    const auto unrelated = QStringLiteral("org.qindaqt.test.Unrelated");
    const auto authority = QStringLiteral("org.qindaqt.test.Authority");
    QVERIFY(f.session.registerService(unrelated));
    QVERIFY(f.session.registerService(authority));
    QVERIFY(f.start());
    QVERIFY(f.session.unregisterService(unrelated)); // still admitted
    QTest::qWait(100);
    QVERIFY(revoked.isEmpty());
    QVERIFY(f.attachment.live());
    f.admitted = false; // the consumer's authority is about to move
    QVERIFY(f.session.unregisterService(authority));
    QTRY_COMPARE(revoked.size(), 1);
    QVERIFY(!f.attachment.live());
    QVERIFY(!f.attachment.recovering());
    QVERIFY(!f.timers.only().isActive());
    // Nothing more is published for the retired selection.
    f.admitted = true;
    QVERIFY(f.session.registerService(unrelated));
    QVERIFY(f.session.unregisterService(unrelated));
    QTest::qWait(100);
    QCOMPARE(revoked.size(), 1);
  }
  void revokeAdmissionAndSessionLossRetireTheSelection() {
    Fixture f;
    QSignalSpy attached(&f.attachment, &CompositorAttachment::attached);
    QVERIFY(f.start());
    f.attachment.revoke();
    QVERIFY(!f.attachment.recovering());
    QVERIFY(!f.timers.only().isActive());
    QVERIFY(f.attachment.attach(f.session.baseService(), socketName()));
    QVERIFY(f.compositor.unregisterService(Service));
    QTRY_VERIFY(f.attachment.recovering());
    f.admitted = false;
    QVERIFY(f.timers.only().fire());
    QVERIFY(!f.attachment.recovering());
    f.admitted = true;
    QVERIFY(f.attachment.attach(f.session.baseService(), socketName()) ==
            false); // no compositor owner: nothing is retained
    QVERIFY(f.compositor.registerService(Service));
    QVERIFY(f.attachment.attach(f.session.baseService(), socketName()));
    QVERIFY(f.compositor.unregisterService(Service));
    QTRY_VERIFY(f.attachment.recovering());
    // Drop the last handle too, or Qt keeps the connection open.
    QDBusConnection::disconnectFromBus(f.session.name());
    f.session = QDBusConnection(QStringLiteral("disconnected-fixture"));
    QTRY_VERIFY(!f.attachment.recovering());
    QVERIFY(f.compositor.registerService(Service));
    QCOMPARE(attached.size(), 3);
    QVERIFY(!f.attachment.live());
  }
};
QTEST_GUILESS_MAIN(AttachmentRecoveryTest)
#include "tst_compositor_attachment_recovery.moc"
