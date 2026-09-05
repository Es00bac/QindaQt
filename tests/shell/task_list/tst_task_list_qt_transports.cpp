// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/operations/qt_task_list_operation_transport.h"
#include "qindaqt/shell/task_list/producer/task_list_facts_producer.h"
#include "qindaqt/shell/task_list/producer/qt_task_list_producer_transport.h"

#include <QDBusConnection>
#include <QProcess>
#include <QSignalSpy>
#include <QtTest>
#include <QUuid>

#include "task_list_producer_test_support.h"

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskList::Operations;
using namespace QindaQt::ShellTaskList::Producer;
using namespace TaskListProducerTest;

namespace {

constexpr auto kServiceName = "org.qindaqt.Compositor";
constexpr auto kObjectPath = "/org/qindaqt/Compositor";
constexpr auto kShellObjectPath = "/org/qindaqt/CompositorShell";

// Every bus in this file is a fresh private dbus-daemon; the host session bus
// and any host compositor are never contacted.
class PrivateSessionBus final {
public:
  ~PrivateSessionBus() {
    if (m_process.state() != QProcess::NotRunning) {
      m_process.terminate();
      if (!m_process.waitForFinished(1'000)) {
        m_process.kill();
        m_process.waitForFinished(1'000);
      }
    }
  }

  bool start(QString *error) {
    m_process.start(QStringLiteral("dbus-daemon"),
                    {QStringLiteral("--session"), QStringLiteral("--nofork"),
                     QStringLiteral("--nopidfile"),
                     QStringLiteral("--print-address=1")});
    if (!m_process.waitForStarted(5'000) ||
        !m_process.waitForReadyRead(5'000)) {
      *error = m_process.errorString();
      return false;
    }
    m_address = QString::fromUtf8(m_process.readLine()).trimmed();
    if (m_address.isEmpty()) {
      *error = QStringLiteral("private dbus-daemon did not publish an address");
      return false;
    }
    return true;
  }

  [[nodiscard]] const QString &address() const noexcept { return m_address; }

private:
  QProcess m_process;
  QString m_address;
};

class FakeCompositor final : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.qindaqt.CompositorShell1")

public:
  explicit FakeCompositor(StandardScene scene, QObject *parent = nullptr)
      : QObject(parent), m_scene(std::move(scene)) {}

public Q_SLOTS:
  Q_SCRIPTABLE QByteArray TaskListSnapshot() const {
    ++factsCalls;
    return m_scene.taskFacts;
  }

Q_SIGNALS:
  Q_SCRIPTABLE void TaskListSnapshotChanged();

public:
  mutable int factsCalls = 0;

private:
  StandardScene m_scene;
};

class FakeControlCompositor final : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Compositor1")

public:
  QByteArray lastSubmit;
  QStringList lastRelease;
  QStringList lastDock;

public Q_SLOTS:
  Q_SCRIPTABLE QByteArray Submit(const QByteArray &requestJson) {
    lastSubmit = requestJson;
    return QByteArrayLiteral("{\"status\":\"committed\",\"revision\":\"2\"}");
  }
  Q_SCRIPTABLE QByteArray ReleaseContainer(const QString &containerId) {
    lastRelease.append(containerId);
    return QByteArrayLiteral("{\"status\":\"released\"}");
  }
  Q_SCRIPTABLE QByteArray DockWindows(const QString &target,
                                      const QString &incoming,
                                      const QString &orientation,
                                      const QString &position, double ratio) {
    lastDock.append({target, incoming, orientation, position,
                     QString::number(ratio)});
    return QByteArrayLiteral("{\"status\":\"docked\"}");
  }

};

QString connectionName(const QString &role) {
  return QStringLiteral("qindaqt-task-list-transport-%1-%2")
      .arg(role, QUuid::createUuid().toString(QUuid::Id128));
}

bool registerCompositor(QDBusConnection &connection, FakeCompositor *object) {
  return connection.registerObject(QString::fromLatin1(kShellObjectPath), object,
                                   QDBusConnection::ExportScriptableSlots |
                                       QDBusConnection::ExportScriptableSignals) &&
         connection.registerService(QString::fromLatin1(kServiceName));
}

bool registerCompositor(QDBusConnection &connection,
                        FakeControlCompositor *object) {
  return connection.registerObject(QString::fromLatin1(kObjectPath), object,
                                   QDBusConnection::ExportScriptableSlots)
      && connection.registerService(QString::fromLatin1(kServiceName));
}

} // namespace

class TaskListQtTransportsTests final : public QObject {
  Q_OBJECT

private slots:
  void initialUnownedCompositorDegradesProducer();
  void producerTransportBindsReadsAndSignalsToTheExactOwner();
  void operationTransportDeliversMutationsToTheExactOwner();
};

// AGENT-NOTE: Review finding P2-1 on rejected candidate 7b6bd8a: resolving an
// initially unowned compositor is a completed unavailable observation, not an
// owner transition that may be suppressed as an empty-to-empty no-op.
void TaskListQtTransportsTests::initialUnownedCompositorDegradesProducer() {
  PrivateSessionBus bus;
  QString busError;
  QVERIFY2(bus.start(&busError), qPrintable(busError));

  const QString clientName = connectionName(QStringLiteral("unowned-client"));
  auto client = QDBusConnection::connectToBus(bus.address(), clientName);
  QVERIFY(client.isConnected());

  {
    TaskListSource source;
    QtTaskListProducerTransport transport(client);
    TaskListFactsProducer producer(transport, source);
    QSignalSpy stateSpy(&producer, &TaskListFactsProducer::stateChanged);
    QString startError;
    QVERIFY2(producer.start(&startError), qPrintable(startError));

    QTRY_COMPARE_WITH_TIMEOUT(source.status(), TaskListSourceStatus::Degraded,
                              5'000);
    QCOMPARE(producer.uniqueOwner(), QString());
    QVERIFY(producer.lastError().contains(QStringLiteral("unavailable")));
    QCOMPARE(stateSpy.size(), 1);
  }

  QDBusConnection::disconnectFromBus(clientName);
}

void TaskListQtTransportsTests::producerTransportBindsReadsAndSignalsToTheExactOwner() {
  PrivateSessionBus bus;
  QString busError;
  QVERIFY2(bus.start(&busError), qPrintable(busError));

  const QString serverAName = connectionName(QStringLiteral("server-a"));
  const QString serverBName = connectionName(QStringLiteral("server-b"));
  const QString clientName = connectionName(QStringLiteral("client"));
  auto serverA = QDBusConnection::connectToBus(bus.address(), serverAName);
  auto serverB = QDBusConnection::connectToBus(bus.address(), serverBName);
  auto client = QDBusConnection::connectToBus(bus.address(), clientName);
  QVERIFY(serverA.isConnected());
  QVERIFY(serverB.isConnected());
  QVERIFY(client.isConnected());

  FakeCompositor compositorA(standardScene());
  QVERIFY(registerCompositor(serverA, &compositorA));
  const QString ownerA = serverA.baseService();
  QVERIFY(ownerA.startsWith(QLatin1Char(':')));

  {
    QtTaskListProducerTransport transport(client);
    QSignalSpy ownerSpy(&transport,
                        &TaskListProducerTransport::serviceOwnerChanged);
    QSignalSpy invalidationSpy(&transport,
                               &TaskListProducerTransport::refreshInvalidated);
    QSignalSpy factsSpy(&transport,
                        &TaskListProducerTransport::factsRead);
    QSignalSpy failureSpy(&transport,
                          &TaskListProducerTransport::refreshFailed);
    QString startError;
    QVERIFY2(transport.start(&startError), qPrintable(startError));
    QTRY_COMPARE_WITH_TIMEOUT(ownerSpy.size(), 1, 5'000);
    QCOMPARE(ownerSpy.constFirst().constFirst().toString(), ownerA);

    // One refresh reaches only the atomic authenticated snapshot.
    transport.requestRefresh(7, ownerA);
    QTRY_COMPARE_WITH_TIMEOUT(factsSpy.size(), 1, 5'000);
    QCOMPARE(factsSpy.constFirst().at(0).toULongLong(), quint64(7));
    QCOMPARE(factsSpy.constFirst().at(1).toString(), ownerA);
    QCOMPARE(compositorA.factsCalls, 1);
    QCOMPARE(failureSpy.size(), 0);

    // A request for a stale owner fails instead of reaching the bus.
    transport.requestRefresh(8, QStringLiteral(":9.9"));
    QTRY_COMPARE_WITH_TIMEOUT(failureSpy.size(), 1, 5'000);

    Q_EMIT compositorA.TaskListSnapshotChanged();
    QTRY_COMPARE_WITH_TIMEOUT(invalidationSpy.size(), 1, 5'000);
    QCOMPARE(invalidationSpy.constFirst().constFirst().toString(), ownerA);

    // Owner loss then replacement: signals from the dead owner must not
    // forward, and the new owner binds reads and signals.
    QVERIFY(serverA.unregisterService(QString::fromLatin1(kServiceName)));
    QTRY_COMPARE_WITH_TIMEOUT(ownerSpy.size(), 2, 5'000);
    QVERIFY(ownerSpy.at(1).constFirst().toString().isEmpty());
    Q_EMIT compositorA.TaskListSnapshotChanged();
    QTest::qWait(50);
    QCOMPARE(invalidationSpy.size(), 1);

    FakeCompositor compositorB(standardScene());
    QVERIFY(registerCompositor(serverB, &compositorB));
    const QString ownerB = serverB.baseService();
    QTRY_COMPARE_WITH_TIMEOUT(ownerSpy.size(), 3, 5'000);
    QCOMPARE(ownerSpy.at(2).constFirst().toString(), ownerB);

    transport.requestRefresh(9, ownerB);
    QTRY_COMPARE_WITH_TIMEOUT(factsSpy.size(), 2, 5'000);
    QCOMPARE(factsSpy.at(1).at(1).toString(), ownerB);
    Q_EMIT compositorB.TaskListSnapshotChanged();
    QTRY_COMPARE_WITH_TIMEOUT(invalidationSpy.size(), 2, 5'000);
    QCOMPARE(invalidationSpy.at(1).constFirst().toString(), ownerB);

    transport.stop();
    QVERIFY(serverB.unregisterService(QString::fromLatin1(kServiceName)));
  }

  serverA.unregisterObject(QString::fromLatin1(kShellObjectPath));
  serverB.unregisterObject(QString::fromLatin1(kShellObjectPath));
  QDBusConnection::disconnectFromBus(clientName);
  QDBusConnection::disconnectFromBus(serverBName);
  QDBusConnection::disconnectFromBus(serverAName);
}

void TaskListQtTransportsTests::operationTransportDeliversMutationsToTheExactOwner() {
  PrivateSessionBus bus;
  QString busError;
  QVERIFY2(bus.start(&busError), qPrintable(busError));

  const QString serverName = connectionName(QStringLiteral("server"));
  const QString clientName = connectionName(QStringLiteral("client"));
  auto server = QDBusConnection::connectToBus(bus.address(), serverName);
  auto client = QDBusConnection::connectToBus(bus.address(), clientName);
  QVERIFY(server.isConnected());
  QVERIFY(client.isConnected());

  FakeControlCompositor compositor;
  QVERIFY(registerCompositor(server, &compositor));
  const QString owner = server.baseService();

  {
    QtTaskListOperationTransport transport(client);
    QSignalSpy replySpy(&transport,
                        &TaskListOperationTransport::operationReplied);
    QSignalSpy failureSpy(&transport,
                          &TaskListOperationTransport::operationFailed);

    QCOMPARE(transport.allocateToken(), quint64(1));
    QCOMPARE(transport.allocateToken(), quint64(2));
    // An empty owner is refused before any bus traffic.
    QVERIFY(!transport.submitTransaction(3, {}, QByteArrayLiteral("{}")));

    QVERIFY(transport.submitTransaction(
        4, owner, QByteArrayLiteral("{\"protocol\":{\"major\":1,\"minor\":1}}")));
    QTRY_COMPARE_WITH_TIMEOUT(replySpy.size(), 1, 5'000);
    QCOMPARE(replySpy.constFirst().at(0).toULongLong(), quint64(4));
    QCOMPARE(replySpy.constFirst().at(1).toString(), owner);
    QTRY_VERIFY_WITH_TIMEOUT(!compositor.lastSubmit.isEmpty(), 5'000);
    QCOMPARE(QString::fromUtf8(replySpy.constFirst().at(2).toByteArray()),
             QStringLiteral("{\"status\":\"committed\",\"revision\":\"2\"}"));

    QVERIFY(transport.releaseContainer(5, owner, QStringLiteral("c1")));
    QTRY_COMPARE_WITH_TIMEOUT(replySpy.size(), 2, 5'000);
    QCOMPARE(compositor.lastRelease, QStringList{QStringLiteral("c1")});

    QVERIFY(transport.dockWindows(6, owner, QStringLiteral("w1"),
                                  QStringLiteral("w2"),
                                  QStringLiteral("horizontal"),
                                  QStringLiteral("second"), 0.5));
    QTRY_COMPARE_WITH_TIMEOUT(replySpy.size(), 3, 5'000);
    // One dock call records its five arguments.
    QCOMPARE(compositor.lastDock.size(), 5);
    QCOMPARE(compositor.lastDock.at(2), QStringLiteral("horizontal"));
    QCOMPARE(failureSpy.size(), 0);

    // A call to a never-existent owner is sent, then fails on the bus.
    QVERIFY(transport.submitTransaction(7, QStringLiteral(":9.99"),
                                        QByteArrayLiteral("{}")));
    QTRY_COMPARE_WITH_TIMEOUT(failureSpy.size(), 1, 5'000);
    QCOMPARE(failureSpy.constFirst().at(0).toULongLong(), quint64(7));
  }

  server.unregisterObject(QString::fromLatin1(kObjectPath));
  QDBusConnection::disconnectFromBus(clientName);
  QDBusConnection::disconnectFromBus(serverName);
}

QTEST_GUILESS_MAIN(TaskListQtTransportsTests)
#include "tst_task_list_qt_transports.moc"
