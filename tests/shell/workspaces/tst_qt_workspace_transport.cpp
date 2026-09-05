// SPDX-License-Identifier: GPL-3.0-or-later

// The authenticated KWin workspace adapter against a fake `org.kde.KWin` on a
// private dbus-daemon. Both ends live in this process, so the bus-daemon PID
// of the fake equals getpid(): the positive case injects that PID, the
// negative case injects a different one. The host session bus is never used.

#include "qindaqt/shell/workspaces/qt_workspace_transport.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QProcess>
#include <QSignalSpy>
#include <QUuid>
#include <QtTest>

#include <unistd.h>

using namespace QindaQt::Shell::Workspaces;

struct FakeDesktopData {
  int position = 0;
  QString id;
  QString name;
};
Q_DECLARE_METATYPE(FakeDesktopData)
using FakeDesktopList = QList<FakeDesktopData>;
Q_DECLARE_METATYPE(FakeDesktopList)

QDBusArgument &operator<<(QDBusArgument &argument, const FakeDesktopData &data)
{
  argument.beginStructure();
  argument << data.position << data.id << data.name;
  argument.endStructure();
  return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, FakeDesktopData &data)
{
  argument.beginStructure();
  argument >> data.position >> data.id >> data.name;
  argument.endStructure();
  return argument;
}

namespace {

class PrivateSessionBus final {
public:
  ~PrivateSessionBus() { stop(); }

  bool start(QString *error)
  {
    const QString address = QStringLiteral("unix:abstract=qindaqt-workspaces-%1")
                                .arg(QUuid::createUuid().toString(QUuid::Id128));
    m_process.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                    {QStringLiteral("--session"), QStringLiteral("--nofork"),
                     QStringLiteral("--nopidfile"),
                     QStringLiteral("--address=%1").arg(address),
                     QStringLiteral("--print-address=1")});
    if (!m_process.waitForStarted(5'000) || !m_process.waitForReadyRead(5'000)) {
      *error = m_process.errorString();
      return false;
    }
    m_address = QString::fromUtf8(m_process.readLine()).trimmed();
    return !m_address.isEmpty();
  }

  void stop()
  {
    if (m_process.state() == QProcess::NotRunning) {
      return;
    }
    m_process.terminate();
    if (!m_process.waitForFinished(1'000)) {
      m_process.kill();
      (void)m_process.waitForFinished(1'000);
    }
  }

  [[nodiscard]] QString address() const { return m_address; }

private:
  QProcess m_process;
  QString m_address;
};

// Fake org.kde.KWin.VirtualDesktopManager at /VirtualDesktopManager.
class FakeDesktopManager final : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.VirtualDesktopManager")
  Q_PROPERTY(uint count READ count)
  Q_PROPERTY(QString current READ current WRITE setCurrent)
  Q_PROPERTY(uint rows READ rows)
  Q_PROPERTY(bool navigationWrappingAround READ navigationWrappingAround)
  Q_PROPERTY(FakeDesktopList desktops READ desktops)

public:
  FakeDesktopList desktopList{{0, QStringLiteral("uuid-a"), QStringLiteral("Alpha")},
                              {1, QStringLiteral("uuid-b"), QStringLiteral("Beta")}};
  QString currentId = QStringLiteral("uuid-a");
  bool rejectNextSet = false;
  QStringList setRequests;

  [[nodiscard]] uint count() const { return static_cast<uint>(desktopList.size()); }
  [[nodiscard]] QString current() const { return currentId; }
  [[nodiscard]] uint rows() const { return 1; }
  [[nodiscard]] bool navigationWrappingAround() const { return true; }
  [[nodiscard]] FakeDesktopList desktops() const { return desktopList; }

  void setCurrent(const QString &id)
  {
    setRequests.append(id);
    for (const FakeDesktopData &desktop : desktopList) {
      if (desktop.id == id) {
        currentId = id;
        Q_EMIT currentChanged(id);
        return;
      }
    }
  }

Q_SIGNALS:
  void currentChanged(const QString &id);
  void countChanged(uint count);
};

// Fake org.kde.KWin at /KWin.
class FakeKWin final : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.kde.KWin")
  Q_PROPERTY(bool showingDesktop READ showingDesktop)

public:
  bool showing = false;
  [[nodiscard]] bool showingDesktop() const { return showing; }

public Q_SLOTS:
  void showDesktop(bool value)
  {
    showing = value;
    Q_EMIT showingDesktopChanged(value);
  }

Q_SIGNALS:
  void showingDesktopChanged(bool showing);
};

struct FakeCompositor {
  QDBusConnection connection;
  FakeDesktopManager desktops;
  FakeKWin kwin;

  explicit FakeCompositor(const QString &address)
      : connection(QDBusConnection::connectToBus(address, QStringLiteral("fake-kwin")))
  {
  }

  bool publish(bool registerPeer)
  {
    const auto options = QDBusConnection::ExportAllProperties
        | QDBusConnection::ExportAllSignals | QDBusConnection::ExportAllSlots;
    if (!connection.registerObject(QStringLiteral("/VirtualDesktopManager"), &desktops, options)
        || !connection.registerObject(QStringLiteral("/KWin"), &kwin, options)) {
      return false;
    }
    if (!connection.registerService(QStringLiteral("org.kde.KWin"))) {
      return false;
    }
    return !registerPeer || connection.registerService(QStringLiteral("org.qindaqt.Compositor"));
  }
};

} // namespace

class QtWorkspaceTransportTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void authenticatedOwnerReadsSwitchesAndShowsDesktop();
  void mismatchedProcessIdNeverBinds();
  void peerServiceFallbackAuthenticatesAndItsAbsenceFailsClosed();
  void requestsForUnboundOwnersFailImmediately();
};

void QtWorkspaceTransportTests::initTestCase()
{
  qDBusRegisterMetaType<FakeDesktopData>();
  qDBusRegisterMetaType<FakeDesktopList>();
}

void QtWorkspaceTransportTests::authenticatedOwnerReadsSwitchesAndShowsDesktop()
{
  PrivateSessionBus bus;
  QString error;
  QVERIFY2(bus.start(&error), qPrintable(error));
  FakeCompositor compositor(bus.address());
  QVERIFY(compositor.publish(false));
  QDBusConnection shell = QDBusConnection::connectToBus(bus.address(), QStringLiteral("shell"));
  QVERIFY(shell.isConnected());

  WorkspaceAuthority authority;
  authority.compositorProcessId = static_cast<qint64>(getpid());
  QtWorkspaceTransport transport(shell, authority);
  QSignalSpy ownerSpy(&transport, &WorkspaceTransport::ownerChanged);
  QSignalSpy snapshotSpy(&transport, &WorkspaceTransport::snapshotReceived);
  QSignalSpy changedSpy(&transport, &WorkspaceTransport::changed);
  QSignalSpy finishedSpy(&transport, &WorkspaceTransport::operationFinished);
  QVERIFY2(transport.start(&error), qPrintable(error));

  QTRY_VERIFY(!ownerSpy.isEmpty() && !ownerSpy.constLast().at(0).toString().isEmpty());
  const QString owner = ownerSpy.constLast().at(0).toString();
  QCOMPARE(owner, compositor.connection.baseService());
  QCOMPARE(transport.boundOwner(), owner);

  transport.requestSnapshot(11, owner);
  QTRY_COMPARE(snapshotSpy.size(), 1);
  QCOMPARE(snapshotSpy.constFirst().at(0).toULongLong(), quint64(11));
  QCOMPARE(snapshotSpy.constFirst().at(1).toString(), owner);
  const auto snapshot = snapshotSpy.constFirst().at(2).value<WorkspaceSnapshot>();
  QCOMPARE(snapshot.desktops.size(), 2);
  QCOMPARE(snapshot.desktops.at(1).name, QStringLiteral("Beta"));
  QCOMPARE(snapshot.currentId, QStringLiteral("uuid-a"));
  QCOMPARE(snapshot.rows, 1u);
  QCOMPARE(snapshot.showingDesktop, false);

  transport.requestSwitch(12, owner, QStringLiteral("uuid-b"));
  QTRY_COMPARE(finishedSpy.size(), 1);
  QCOMPARE(finishedSpy.constLast().at(0).toULongLong(), quint64(12));
  QCOMPARE(finishedSpy.constLast().at(2).toBool(), true);
  QCOMPARE(compositor.desktops.setRequests, QStringList{QStringLiteral("uuid-b")});
  QTRY_VERIFY(!changedSpy.isEmpty());
  QCOMPARE(changedSpy.constLast().at(0).toString(), owner);

  transport.requestShowDesktop(13, owner, true);
  QTRY_COMPARE(finishedSpy.size(), 2);
  QCOMPARE(finishedSpy.constLast().at(2).toBool(), true);
  QCOMPARE(compositor.kwin.showing, true);
  const int changes = static_cast<int>(changedSpy.size());
  QTRY_VERIFY(changedSpy.size() > changes - 1);

  transport.requestSnapshot(14, owner);
  QTRY_COMPARE(snapshotSpy.size(), 2);
  const auto updated = snapshotSpy.constLast().at(2).value<WorkspaceSnapshot>();
  QCOMPARE(updated.currentId, QStringLiteral("uuid-b"));
  QCOMPARE(updated.showingDesktop, true);

  // A request to the well-known name or a foreign unique name is refused
  // locally: only the bound unique owner is ever called.
  transport.requestSwitch(15, QStringLiteral("org.kde.KWin"), QStringLiteral("uuid-a"));
  QTRY_COMPARE(finishedSpy.size(), 3);
  QCOMPARE(finishedSpy.constLast().at(2).toBool(), false);
  QCOMPARE(finishedSpy.constLast().at(3).toString(), QStringLiteral("owner-not-bound"));
  QCOMPARE(compositor.desktops.setRequests.size(), 1);

  transport.stop();
  QCOMPARE(ownerSpy.constLast().at(0).toString(), QString{});
  QCOMPARE(ownerSpy.constLast().at(1).toString(),
           QStringLiteral("workspace-transport-stopped"));
}

void QtWorkspaceTransportTests::mismatchedProcessIdNeverBinds()
{
  PrivateSessionBus bus;
  QString error;
  QVERIFY2(bus.start(&error), qPrintable(error));
  FakeCompositor compositor(bus.address());
  QVERIFY(compositor.publish(false));
  QDBusConnection shell = QDBusConnection::connectToBus(bus.address(), QStringLiteral("shell-2"));

  WorkspaceAuthority authority;
  authority.compositorProcessId = static_cast<qint64>(getpid()) + 1;
  QtWorkspaceTransport transport(shell, authority);
  QSignalSpy ownerSpy(&transport, &WorkspaceTransport::ownerChanged);
  QSignalSpy snapshotSpy(&transport, &WorkspaceTransport::snapshotReceived);
  QSignalSpy failedSpy(&transport, &WorkspaceTransport::snapshotFailed);
  QVERIFY2(transport.start(&error), qPrintable(error));
  QTRY_VERIFY(!ownerSpy.isEmpty());
  QCOMPARE(ownerSpy.constLast().at(0).toString(), QString{});
  QCOMPARE(ownerSpy.constLast().at(1).toString(),
           QStringLiteral("compositor-identity-mismatch"));
  QVERIFY(transport.boundOwner().isEmpty());

  transport.requestSnapshot(1, compositor.connection.baseService());
  QTRY_COMPARE(failedSpy.size(), 1);
  QCOMPARE(failedSpy.constLast().at(2).toString(), QStringLiteral("owner-not-bound"));
  QVERIFY(snapshotSpy.isEmpty());
  transport.stop();
}

void QtWorkspaceTransportTests::peerServiceFallbackAuthenticatesAndItsAbsenceFailsClosed()
{
  PrivateSessionBus bus;
  QString error;
  QVERIFY2(bus.start(&error), qPrintable(error));
  FakeCompositor compositor(bus.address());
  QVERIFY(compositor.publish(false));
  QDBusConnection shell = QDBusConnection::connectToBus(bus.address(), QStringLiteral("shell-3"));

  WorkspaceAuthority authority; // no PID; default peer service name
  QtWorkspaceTransport transport(shell, authority);
  QSignalSpy ownerSpy(&transport, &WorkspaceTransport::ownerChanged);
  QVERIFY2(transport.start(&error), qPrintable(error));
  QTRY_VERIFY(!ownerSpy.isEmpty());
  QCOMPARE(ownerSpy.constLast().at(0).toString(), QString{});
  QCOMPARE(ownerSpy.constLast().at(1).toString(),
           QStringLiteral("compositor-peer-unavailable"));

  // The peer appears in the same process as KWin: the watcher re-resolves
  // and the PIDs now match.
  QVERIFY(compositor.connection.registerService(QStringLiteral("org.qindaqt.Compositor")));
  QTRY_VERIFY(!ownerSpy.constLast().at(0).toString().isEmpty());
  QCOMPARE(ownerSpy.constLast().at(0).toString(), compositor.connection.baseService());

  // Losing the peer drops the binding again.
  QVERIFY(compositor.connection.unregisterService(QStringLiteral("org.qindaqt.Compositor")));
  QTRY_VERIFY(ownerSpy.constLast().at(0).toString().isEmpty());
  transport.stop();
}

void QtWorkspaceTransportTests::requestsForUnboundOwnersFailImmediately()
{
  PrivateSessionBus bus;
  QString error;
  QVERIFY2(bus.start(&error), qPrintable(error));
  QDBusConnection shell = QDBusConnection::connectToBus(bus.address(), QStringLiteral("shell-4"));
  WorkspaceAuthority authority;
  authority.compositorProcessId = static_cast<qint64>(getpid());
  QtWorkspaceTransport transport(shell, authority);
  QSignalSpy ownerSpy(&transport, &WorkspaceTransport::ownerChanged);
  QSignalSpy finishedSpy(&transport, &WorkspaceTransport::operationFinished);
  QVERIFY2(transport.start(&error), qPrintable(error));
  QTRY_VERIFY(!ownerSpy.isEmpty());
  QCOMPARE(ownerSpy.constLast().at(1).toString(), QStringLiteral("compositor-unavailable"));
  transport.requestShowDesktop(1, QStringLiteral(":1.42"), true);
  QCOMPARE(finishedSpy.size(), 1);
  QCOMPARE(finishedSpy.constLast().at(2).toBool(), false);
  transport.stop();
}

QTEST_GUILESS_MAIN(QtWorkspaceTransportTests)
#include "tst_qt_workspace_transport.moc"
