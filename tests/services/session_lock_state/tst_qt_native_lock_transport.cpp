// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/compositor_names/compositor_names.h"
#include "qindaqt/services/session_lock_state/native_lock_state_monitor.h"
#include "qindaqt/services/session_lock_state/qt_native_lock_transport.h"
#include "support/private_session_bus.h"
#include <QCoreApplication>
#include <QDBusMessage>
#include <QProcess>
#include <QSignalSpy>
#include <QUuid>
#include <QtTest>
using namespace QindaQt::Services::SessionLockState;
namespace {
using PrivateBus = QindaQt::TestSupport::PrivateSessionBus;
// Explicit dummy authority confined to this test's private bus and admission
// closure; this object is never part of the product service or installed.
class NativeBackend final : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.qindaqt.KWin.NativeLock1")
  Q_PROPERTY(bool Locked READ locked)
  Q_PROPERTY(bool Protected READ protectedPresentation)
public:
  bool locked() const { return m_locked; }
  bool protectedPresentation() const { return m_protected; }
  void set(bool locked, bool protectedPresentation) {
    if (m_locked != locked) {
      m_locked = locked;
      Q_EMIT lockedChanged(locked);
    }
    if (m_protected != protectedPresentation) {
      m_protected = protectedPresentation;
      Q_EMIT protectedChanged(protectedPresentation);
    }
  }
  bool m_locked = false, m_protected = false;
Q_SIGNALS:
  void lockedChanged(bool locked);
  void protectedChanged(bool protectedPresentation);
};
QString service() { return QString(QindaQt::CompositorNames::service); }
QString path() { return QString(QindaQt::CompositorNames::nativeLockPath); }
QString interface() {
  return QString(QindaQt::CompositorNames::nativeLockInterface);
}
bool expose(QDBusConnection &connection, NativeBackend &backend) {
  return connection.registerObject(path(), &backend,
                                   QDBusConnection::ExportAllProperties |
                                       QDBusConnection::ExportAllSignals) &&
         connection.registerService(service());
}
} // namespace
class NativeTransportTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void admittedOwnerProtectionAndReadOnly() {
    PrivateBus bus;
    QVERIFY(bus.start());
    auto server = bus.connect("native-server-"),
         client = bus.connect("native-client-"),
         hostile = bus.connect("native-hostile-");
    QVERIFY(server.isConnected() && client.isConnected() &&
            hostile.isConnected());
    NativeBackend backend;
    QVERIFY(expose(server, backend));
    bool live = true;
    QtNativeLockTransport transport(client);
    NativeLockStateMonitor monitor(
        transport, [&](const QString &owner, quint64 pid) {
          return live && owner == server.baseService() &&
                 pid == quint64(QCoreApplication::applicationPid());
        });
    QVERIFY(monitor.start());
    QTRY_COMPARE(monitor.state(), LockState::Unlocked);
    // A genuine other same-UID/same-PID bus client cannot forge owner signals.
    auto forged = QDBusMessage::createSignal(path(), interface(),
                                             QStringLiteral("lockedChanged"));
    forged << true;
    QVERIFY(hostile.send(forged));
    QTest::qWait(20);
    QCOMPARE(monitor.state(), LockState::Unlocked);
    backend.set(true, false);
    QTRY_COMPARE(monitor.state(), LockState::Locking);
    QVERIFY(!monitor.presentationProtected());
    backend.set(true, true);
    QTRY_COMPARE(monitor.state(), LockState::Locked);
    QVERIFY(monitor.presentationProtected());
    backend.set(false, false);
    QTRY_COMPARE(monitor.state(), LockState::Unlocked);
    QCOMPARE(transport.metaObject()->indexOfMethod("RequestLock()"), -1);
    live = false;
    QVERIFY(!monitor.contentMayBeShown());
    monitor.refresh();
    QTRY_COMPARE(monitor.state(), LockState::Unknown);
    monitor.stop();
  }
  void samePidReplacementDenied() {
    PrivateBus bus;
    QVERIFY(bus.start());
    auto original = bus.connect("native-original-"),
         replacement = bus.connect("native-replacement-"),
         client = bus.connect("native-client-");
    NativeBackend first, second;
    QVERIFY(expose(original, first));
    const auto admittedOwner = original.baseService();
    QtNativeLockTransport transport(client);
    NativeLockStateMonitor monitor(
        transport, [&](const QString &owner, quint64 pid) {
          return owner == admittedOwner &&
                 pid == quint64(QCoreApplication::applicationPid());
        });
    QVERIFY(monitor.start());
    QTRY_COMPARE(monitor.state(), LockState::Unlocked);
    QVERIFY(original.unregisterService(service()));
    QTRY_COMPARE(monitor.state(), LockState::Unknown);
    QVERIFY(expose(replacement, second));
    QTest::qWait(50);
    QCOMPARE(monitor.state(), LockState::Unknown);
    QVERIFY(!monitor.contentMayBeShown());
    // Both peers have this process's real daemon PID. Exact accepted owner
    // lineage is necessary; server inode/PID equality alone is insufficient.
    first.set(false, false);
    QTest::qWait(20);
    QCOMPARE(monitor.state(), LockState::Unknown);
    monitor.stop();
  }
  void deniedAdmissionAndBusLoss() {
    PrivateBus bus;
    QVERIFY(bus.start());
    auto server = bus.connect("native-server-"),
         client = bus.connect("native-client-");
    NativeBackend backend;
    QVERIFY(expose(server, backend));
    QtNativeLockTransport denied(client);
    NativeLockStateMonitor refusal(
        denied, [](const QString &, quint64) { return false; });
    QVERIFY(refusal.start());
    QTest::qWait(50);
    QCOMPARE(refusal.state(), LockState::Unknown);
    QtNativeLockTransport transport(client);
    NativeLockStateMonitor monitor(
        transport, [&](const QString &owner, quint64 pid) {
          return owner == server.baseService() &&
                 pid == quint64(QCoreApplication::applicationPid());
        });
    QVERIFY(monitor.start());
    QTRY_COMPARE(monitor.state(), LockState::Unlocked);
    bus.stop();
    QTRY_COMPARE(monitor.state(), LockState::Unknown);
    QVERIFY(!monitor.contentMayBeShown());
    QString error;
    QVERIFY(!monitor.start(&error));
    QVERIFY(!error.isEmpty());
    monitor.stop();
    refusal.stop();
  }
};
QTEST_GUILESS_MAIN(NativeTransportTests)
#include "tst_qt_native_lock_transport.moc"
