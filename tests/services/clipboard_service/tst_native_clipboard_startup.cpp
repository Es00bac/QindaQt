// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_clipboard_lock_observer.h"
#include "support/fake_wayland_adapter.h"
#include "support/native_clipboard_fixture.h"

#include <qindaqt/services/clipboard_model/clipboard_descriptor.h>
#include <qindaqt/services/clipboard_service/clipboard_host.h>
#include <qindaqt/services/session_lock_state/qt_session_lock_transport.h>
#include <qindaqt/services/session_lock_state/session_lock_state_monitor.h>
#include <qindaqt/services/session_lock_state/session_lock_transport.h>

#include <QtCore/QTimer>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

using namespace QindaQt::Services;
using QindaQt::Tests::NativeClipboardFixture;
using QindaQt::Tests::ClipboardNativeBackend;

namespace {
ClipboardModel::ClipboardValue value() {
    return {{{QStringLiteral("text/plain"), QByteArray("synthetic unlocked login")}}};
}
qsizetype entries(const Clipboard::Snapshot &snapshot) {
    return ClipboardModel::decodeDescriptorList(snapshot.descriptorList).descriptors.size();
}
void bind(Clipboard::NativeClipboardLockObserver &observer, Clipboard::ClipboardHost &host) {
    QObject::connect(&observer, &Clipboard::NativeClipboardLockObserver::contentMayBeShownChanged,
                     &host, [&host](bool allowed) { host.setUnlocked(allowed); });
    host.setHistoryOptIn(true);
}
}

class NativeClipboardStartupTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void oldSplitOwnerCompositionRefusesUnlockedHistory() {
        NativeClipboardFixture fixture;
        QVERIFY(fixture.start());
        QVERIFY(fixture.session.baseService() != fixture.compositor.baseService());
        SessionLockState::QtSessionLockTransport transport(fixture.client);
        SessionLockState::SessionLockStateMonitor legacy(transport, getpid());
        QSignalSpy resolved(&transport, &SessionLockState::SessionLockTransport::serviceOwnerResolved);
        FakeWaylandAdapter adapter;
        Clipboard::ClipboardHost oldHost(&adapter, 11);
        connect(&legacy, &SessionLockState::SessionLockStateMonitor::contentMayBeShownChanged,
                &oldHost, [&oldHost](bool allowed) { oldHost.setUnlocked(allowed); });
        oldHost.setHistoryOptIn(true);
        QVERIFY(legacy.start());
        QTRY_COMPARE(resolved.size(), 3);
        QCOMPARE(legacy.state(), SessionLockState::LockState::Unknown);
        QVERIFY(oldHost.snapshot().historyEnabled);
        QVERIFY(!oldHost.snapshot().privacyAllowed);
        QVERIFY(!adapter.captureEnabled);
        adapter.offer(value());
        QCOMPARE(entries(oldHost.snapshot()), 0);
    }
    void separateSessionOwnerNativeReceiptEnablesHistory() {
        NativeClipboardFixture fixture;
        QVERIFY(fixture.start());
        Clipboard::NativeClipboardLockObserver observer(
            fixture.client, getpid(), fixture.runtime.path(), fixture.basename);
        FakeWaylandAdapter adapter;
        Clipboard::ClipboardHost host(&adapter, 12, [&] { return observer.contentMayBeShown(); });
        bind(observer, host);
        QVERIFY(!host.snapshot().privacyAllowed);
        QVERIFY(observer.start());
        QTRY_VERIFY(host.snapshot().privacyAllowed);
        QVERIFY(adapter.captureEnabled);
        adapter.offer(value());
        QCOMPARE(entries(host.snapshot()), 1);
        fixture.backend->setState(true, false);
        QTRY_VERIFY(!host.snapshot().privacyAllowed);
        QCOMPARE(entries(host.snapshot()), 0);
        QVERIFY(!adapter.captureEnabled);
        fixture.backend->setState(true, true);
        QTest::qWait(30);
        QVERIFY(!host.snapshot().privacyAllowed);
        fixture.backend->setState(false, false);
        QTRY_VERIFY(host.snapshot().privacyAllowed);
        QVERIFY(adapter.captureEnabled);
        QCOMPARE(entries(host.snapshot()), 0);
        observer.stop();
        QVERIFY(!host.snapshot().privacyAllowed);
        QVERIFY(!adapter.captureEnabled);
        QVERIFY(!observer.start()); // stopped incarnation cannot reuse its receipt
    }
    void wrongKernelPeerPidCannotEnableHistory() {
        NativeClipboardFixture fixture;
        QVERIFY(fixture.start());
        Clipboard::NativeClipboardLockObserver observer(
            fixture.client, qint64(getpid()) + 1, fixture.runtime.path(), fixture.basename);
        FakeWaylandAdapter adapter;
        Clipboard::ClipboardHost host(&adapter, 13, [&] { return observer.contentMayBeShown(); });
        bind(observer, host);
        QVERIFY(!observer.start());
        host.setUnlocked(true); // an unqualified signal is insufficient
        QVERIFY(!host.snapshot().privacyAllowed);
        QVERIFY(!adapter.captureEnabled);
        QCOMPARE(entries(host.snapshot()), 0);
    }
    void receiptAuthenticationFailure_data() {
        QTest::addColumn<QString>("mode");
        QTest::newRow("missing") << QStringLiteral("missing");
        QTest::newRow("wrong-nonce") << QStringLiteral("wrong-nonce");
        QTest::newRow("duplicate") << QStringLiteral("duplicate");
        QTest::newRow("forged-owner-same-pid") << QStringLiteral("forged-owner");
    }
    void receiptAuthenticationFailure() {
        QFETCH(QString, mode);
        NativeClipboardFixture fixture;
        QVERIFY(fixture.start());
        fixture.backend->receiptMode = mode;
        Clipboard::NativeClipboardLockObserver observer(
            fixture.client, getpid(), fixture.runtime.path(), fixture.basename);
        FakeWaylandAdapter adapter;
        Clipboard::ClipboardHost host(&adapter, 14, [&] { return observer.contentMayBeShown(); });
        bind(observer, host);
        QVERIFY(observer.start());
        QTRY_COMPARE(fixture.backend->requests, 1);
        QTest::qWait(300);
        QVERIFY(!observer.contentMayBeShown());
        host.setUnlocked(true);
        QVERIFY(!host.snapshot().privacyAllowed);
        QVERIFY(!adapter.captureEnabled);
        QCOMPARE(entries(host.snapshot()), 0);
    }
    void revokedSessionPurgesBeforeQueuedWatchers() {
        NativeClipboardFixture fixture;
        QVERIFY(fixture.start());
        Clipboard::NativeClipboardLockObserver observer(
            fixture.client, getpid(), fixture.runtime.path(), fixture.basename);
        FakeWaylandAdapter adapter;
        Clipboard::ClipboardHost host(&adapter, 15, [&] { return observer.contentMayBeShown(); });
        bind(observer, host);
        QVERIFY(observer.start());
        QTRY_VERIFY(host.snapshot().privacyAllowed);
        adapter.offer(value());
        const auto before = host.snapshot();
        QCOMPARE(entries(before), 1);
        QVERIFY(fixture.session.unregisterService(QStringLiteral("org.qindaqt.Session1")));
        QVERIFY(fixture.attacker.registerService(QStringLiteral("org.qindaqt.Session1")));
        // No event-loop turn and no observer getter precedes the public snapshot.
        const auto revoked = host.snapshot();
        QVERIFY(!revoked.privacyAllowed);
        QCOMPARE(revoked.generation, before.generation + 1);
        QCOMPARE(entries(revoked), 0);
        QVERIFY(!adapter.captureEnabled);
        fixture.backend->setState(false, false);
        QTest::qWait(150);
        QVERIFY(!host.snapshot().privacyAllowed);
    }
    void samePidCompositorReplacementCannotReuseReceipt() {
        NativeClipboardFixture fixture;
        QVERIFY(fixture.start());
        Clipboard::NativeClipboardLockObserver observer(
            fixture.client, getpid(), fixture.runtime.path(), fixture.basename);
        FakeWaylandAdapter adapter;
        Clipboard::ClipboardHost host(&adapter, 16, [&] { return observer.contentMayBeShown(); });
        bind(observer, host);
        QVERIFY(observer.start());
        QTRY_VERIFY(host.snapshot().privacyAllowed);
        adapter.offer(value());
        const auto old = host.snapshot();
        QVERIFY(fixture.compositor.unregisterService(QindaQt::Tests::compositorService()));
        ClipboardNativeBackend replacement(fixture.attacker, fixture.client);
        QVERIFY(replacement.expose());
        const auto revoked = host.snapshot();
        QVERIFY(!revoked.privacyAllowed);
        QCOMPARE(revoked.generation, old.generation + 1);
        QCOMPARE(entries(revoked), 0);
        QTest::qWait(150);
        QVERIFY(!observer.contentMayBeShown());
        QVERIFY(!adapter.captureEnabled);
    }
    void lateNativeObjectRequiresFreshNonceReceipt() {
        NativeClipboardFixture fixture;
        QVERIFY(fixture.start(false));
        Clipboard::NativeClipboardLockObserver observer(
            fixture.client, getpid(), fixture.runtime.path(), fixture.basename);
        FakeWaylandAdapter adapter;
        Clipboard::ClipboardHost host(&adapter, 17, [&] { return observer.contentMayBeShown(); });
        bind(observer, host);
        QVERIFY(observer.start());
        QTest::qWait(30);
        QVERIFY(!host.snapshot().privacyAllowed);
        QVERIFY(!adapter.captureEnabled);
        QCOMPARE(fixture.backend->requests, 0);
        QVERIFY(fixture.backend->exposeObject());
        QTRY_VERIFY_WITH_TIMEOUT(host.snapshot().privacyAllowed, 3000);
        QCOMPARE(fixture.backend->requests, 1);
        QVERIFY(adapter.captureEnabled);
    }
    void lateFirstSessionOwnerEnablesWithoutRebinding() {
        NativeClipboardFixture fixture;
        QVERIFY(fixture.start(true, false));
        Clipboard::NativeClipboardLockObserver observer(
            fixture.client, getpid(), fixture.runtime.path(), fixture.basename);
        FakeWaylandAdapter adapter;
        Clipboard::ClipboardHost host(&adapter, 18, [&] { return observer.contentMayBeShown(); });
        bind(observer, host);
        const bool observing = observer.start();
        QVERIFY(!host.snapshot().privacyAllowed);
        QVERIFY(!adapter.captureEnabled);
        QCOMPARE(fixture.backend->requests, 0);
        // Real normal-login ordering: resident process exists first, Session1
        // is advertised later. Preserve this exact row against immutable064.
        QVERIFY(fixture.session.registerService(QStringLiteral("org.qindaqt.Session1")));
        QTRY_VERIFY_WITH_TIMEOUT(host.snapshot().privacyAllowed, 3000);
        QVERIFY(observing);
        QVERIFY(adapter.captureEnabled);
        adapter.offer(value());
        QCOMPARE(entries(host.snapshot()), 1);
    }
    void firstOwnerAfterShortReceiptRetryBudgetStillAdmits() {
        NativeClipboardFixture fixture;
        QVERIFY(fixture.start(true, false));
        Clipboard::NativeClipboardLockObserver observer(
            fixture.client, getpid(), fixture.runtime.path(), fixture.basename);
        FakeWaylandAdapter adapter;
        Clipboard::ClipboardHost host(&adapter, 20, [&] { return observer.contentMayBeShown(); });
        bind(observer, host);
        const bool observing = observer.start();
        QVERIFY(!host.snapshot().privacyAllowed);
        bool published = false;
        QTimer::singleShot(2500, &observer, [&] {
            published = fixture.session.registerService(QStringLiteral("org.qindaqt.Session1"));
        });
        QTRY_VERIFY_WITH_TIMEOUT(host.snapshot().privacyAllowed, 6500);
        QVERIFY(observing && published);
        QVERIFY(adapter.captureEnabled);
        QCOMPARE(fixture.backend->requests, 1);
    }
    void absentFirstOwnerTimeoutIsTerminal() {
        NativeClipboardFixture fixture;
        QVERIFY(fixture.start(true, false));
        Clipboard::NativeClipboardLockObserver observer(
            fixture.client, getpid(), fixture.runtime.path(), fixture.basename);
        FakeWaylandAdapter adapter;
        Clipboard::ClipboardHost host(&adapter, 19, [&] { return observer.contentMayBeShown(); });
        bind(observer, host);
        QVERIFY(observer.start());
        QTRY_VERIFY_WITH_TIMEOUT(!observer.start(), 35'000);
        QVERIFY(!host.snapshot().privacyAllowed);
        QCOMPARE(fixture.backend->requests, 0);
        QVERIFY(fixture.session.registerService(QStringLiteral("org.qindaqt.Session1")));
        QTest::qWait(150);
        QVERIFY(!observer.start());
        QVERIFY(!observer.contentMayBeShown());
        QVERIFY(!adapter.captureEnabled);
        QCOMPARE(fixture.backend->requests, 0);
        QCOMPARE(entries(host.snapshot()), 0);
    }
    void stopCancelsPendingFirstOwnerObservation() {
        NativeClipboardFixture fixture;
        QVERIFY(fixture.start(true, false));
        Clipboard::NativeClipboardLockObserver observer(
            fixture.client, getpid(), fixture.runtime.path(), fixture.basename);
        QVERIFY(observer.start());
        observer.stop();
        QVERIFY(fixture.session.registerService(QStringLiteral("org.qindaqt.Session1")));
        QTest::qWait(150);
        QVERIFY(!observer.start());
        QVERIFY(!observer.contentMayBeShown());
        QCOMPARE(fixture.backend->requests, 0);
    }
    void foreignSocketBasenameFailsClosed() {
        NativeClipboardFixture fixture;
        QVERIFY(fixture.start());
        Clipboard::NativeClipboardLockObserver observer(
            fixture.client, getpid(), fixture.runtime.path(), QStringLiteral("wayland-0"));
        QVERIFY(!observer.start());
        QVERIFY(!observer.contentMayBeShown());
    }
};

QTEST_GUILESS_MAIN(NativeClipboardStartupTest)
#include "tst_native_clipboard_startup.moc"
