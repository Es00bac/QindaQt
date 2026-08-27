// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/session_lock_state/qt_session_lock_transport.h"
#include "qindaqt/services/session_lock_state/session_lock_state_monitor.h"

#include <QCoreApplication>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTimer>
#include <QtTest>
#include <QUuid>

using namespace QindaQt::Services::SessionLockState;

namespace {

class PrivateSessionBus final {
public:
    ~PrivateSessionBus() { stop(); }

    bool start(QString *error)
    {
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
            *error = QStringLiteral("private dbus-daemon published no address");
            return false;
        }
        return true;
    }

    void stop() noexcept
    {
        if (m_process.state() == QProcess::NotRunning) {
            return;
        }
        m_process.terminate();
        if (!m_process.waitForFinished(1'000)) {
            m_process.kill();
            m_process.waitForFinished(1'000);
        }
    }

    const QString &address() const noexcept { return m_address; }

private:
    QProcess m_process;
    QString m_address;
};

class ScreenSaverBackend final : public QObject {
    Q_OBJECT

public:
    bool active = false;
};

class FreedesktopScreenSaverAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.ScreenSaver")

public:
    explicit FreedesktopScreenSaverAdaptor(ScreenSaverBackend *backend)
        : QDBusAbstractAdaptor(backend)
        , m_backend(backend)
    {
    }

public Q_SLOTS:
    bool GetActive() const { return m_backend->active; }

Q_SIGNALS:
    void ActiveChanged(bool active);

private:
    ScreenSaverBackend *m_backend;
};

class KdeScreenSaverAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.screensaver")

public:
    explicit KdeScreenSaverAdaptor(ScreenSaverBackend *backend)
        : QDBusAbstractAdaptor(backend)
    {
    }

Q_SIGNALS:
    void AboutToLock();
};

QString connectionName(const QString &role)
{
    return QStringLiteral("qindaqt-lock-state-%1-%2")
        .arg(role, QUuid::createUuid().toString(QUuid::Id128));
}

bool registerAllNames(QDBusConnection &connection)
{
    return connection.registerService(QStringLiteral("org.qindaqt.Compositor")) &&
           connection.registerService(
               QStringLiteral("org.freedesktop.ScreenSaver")) &&
           connection.registerService(QStringLiteral("org.kde.screensaver"));
}

} // namespace

class QtSessionLockTransportTests final : public QObject {
    Q_OBJECT

private slots:
    void authenticatesRetriesAndUsesBothSignalInterfaces();
    void rejectsSplitUniqueOwnersAndRecoversAfterReunification();
    void daemonDisconnectImmediatelyRevokesUnlockedAuthority();
};

void QtSessionLockTransportTests::
    authenticatesRetriesAndUsesBothSignalInterfaces()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    const QString serverName = connectionName(QStringLiteral("server"));
    const QString intruderName = connectionName(QStringLiteral("intruder"));
    const QString clientName = connectionName(QStringLiteral("client"));
    auto server = QDBusConnection::connectToBus(bus.address(), serverName);
    auto intruder = QDBusConnection::connectToBus(bus.address(), intruderName);
    auto client = QDBusConnection::connectToBus(bus.address(), clientName);
    QVERIFY(server.isConnected());
    QVERIFY(intruder.isConnected());
    QVERIFY(client.isConnected());
    QVERIFY(registerAllNames(server));

    QtSessionLockTransport transport(client);
    SessionLockStateMonitor monitor(
        transport, QCoreApplication::applicationPid(),
        SessionLockRetryPolicy{{100, 200, 300}});
    QSignalSpy failures(&transport, &SessionLockTransport::requestFailed);
    QVERIFY2(monitor.start(&error), qPrintable(error));

    // The service names intentionally precede object export. The authenticated
    // monitor must fail closed, observe UnknownObject/UnknownMethod, and retry.
    QTRY_VERIFY_WITH_TIMEOUT(!failures.isEmpty(), 5'000);
    const auto failure = failures.constFirst();
    QCOMPARE(failure.at(2).value<LockRequest>(), LockRequest::ActiveState);
    QVERIFY(failure.at(5).toString().endsWith(QStringLiteral("UnknownObject")) ||
            failure.at(5).toString().endsWith(QStringLiteral("UnknownMethod")));
    QCOMPARE(monitor.state(), LockState::Unknown);

    ScreenSaverBackend backend;
    auto *freedesktop = new FreedesktopScreenSaverAdaptor(&backend);
    auto *kde = new KdeScreenSaverAdaptor(&backend);
    QVERIFY(server.registerObject(QStringLiteral("/ScreenSaver"), &backend,
                                  QDBusConnection::ExportAdaptors));
    QTRY_COMPARE_WITH_TIMEOUT(monitor.state(), LockState::Unlocked, 5'000);

    ScreenSaverBackend intruderBackend;
    auto *intruderFreedesktop =
        new FreedesktopScreenSaverAdaptor(&intruderBackend);
    auto *intruderKde = new KdeScreenSaverAdaptor(&intruderBackend);
    QVERIFY(intruder.registerObject(QStringLiteral("/ScreenSaver"),
                                    &intruderBackend,
                                    QDBusConnection::ExportAdaptors));
    Q_EMIT intruderKde->AboutToLock();
    Q_EMIT intruderFreedesktop->ActiveChanged(true);
    QTest::qWait(50);
    QCOMPARE(monitor.state(), LockState::Unlocked);

    Q_EMIT kde->AboutToLock();
    QTRY_COMPARE_WITH_TIMEOUT(monitor.state(), LockState::Locking, 5'000);
    backend.active = true;
    Q_EMIT freedesktop->ActiveChanged(true);
    QTRY_COMPARE_WITH_TIMEOUT(monitor.state(), LockState::Locked, 5'000);
    backend.active = false;
    Q_EMIT freedesktop->ActiveChanged(false);
    QTRY_COMPARE_WITH_TIMEOUT(monitor.state(), LockState::Unlocked, 5'000);

    monitor.stop();
    intruder.unregisterObject(QStringLiteral("/ScreenSaver"));
    server.unregisterObject(QStringLiteral("/ScreenSaver"));
    QDBusConnection::disconnectFromBus(clientName);
    QDBusConnection::disconnectFromBus(intruderName);
    QDBusConnection::disconnectFromBus(serverName);
    client = QDBusConnection(QStringLiteral("released-client"));
    intruder = QDBusConnection(QStringLiteral("released-intruder"));
    server = QDBusConnection(QStringLiteral("released-server"));
    bus.stop();
}

void QtSessionLockTransportTests::
    rejectsSplitUniqueOwnersAndRecoversAfterReunification()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    const QString primaryName = connectionName(QStringLiteral("primary"));
    const QString splitName = connectionName(QStringLiteral("split"));
    const QString clientName = connectionName(QStringLiteral("client"));
    auto primary = QDBusConnection::connectToBus(bus.address(), primaryName);
    auto split = QDBusConnection::connectToBus(bus.address(), splitName);
    auto client = QDBusConnection::connectToBus(bus.address(), clientName);
    QVERIFY(primary.isConnected());
    QVERIFY(split.isConnected());
    QVERIFY(client.isConnected());
    QVERIFY(primary.registerService(QStringLiteral("org.qindaqt.Compositor")));
    QVERIFY(primary.registerService(
        QStringLiteral("org.freedesktop.ScreenSaver")));
    QVERIFY(split.registerService(QStringLiteral("org.kde.screensaver")));

    ScreenSaverBackend backend;
    new FreedesktopScreenSaverAdaptor(&backend);
    new KdeScreenSaverAdaptor(&backend);
    QVERIFY(primary.registerObject(QStringLiteral("/ScreenSaver"), &backend,
                                   QDBusConnection::ExportAdaptors));

    QtSessionLockTransport transport(client);
    SessionLockStateMonitor monitor(transport, QCoreApplication::applicationPid());
    QVERIFY2(monitor.start(&error), qPrintable(error));
    QTest::qWait(100);
    QCOMPARE(monitor.state(), LockState::Unknown);
    QVERIFY(!monitor.contentMayBeShown());

    QVERIFY(split.unregisterService(QStringLiteral("org.kde.screensaver")));
    QVERIFY(primary.registerService(QStringLiteral("org.kde.screensaver")));
    QTRY_COMPARE_WITH_TIMEOUT(monitor.state(), LockState::Unlocked, 5'000);

    QVERIFY(primary.unregisterService(QStringLiteral("org.kde.screensaver")));
    QTRY_COMPARE_WITH_TIMEOUT(monitor.state(), LockState::Unknown, 5'000);
    QVERIFY(!monitor.contentMayBeShown());

    monitor.stop();
    primary.unregisterObject(QStringLiteral("/ScreenSaver"));
    QDBusConnection::disconnectFromBus(clientName);
    QDBusConnection::disconnectFromBus(splitName);
    QDBusConnection::disconnectFromBus(primaryName);
    client = QDBusConnection(QStringLiteral("released-client"));
    split = QDBusConnection(QStringLiteral("released-split"));
    primary = QDBusConnection(QStringLiteral("released-primary"));
    bus.stop();
}

void QtSessionLockTransportTests::
    daemonDisconnectImmediatelyRevokesUnlockedAuthority()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    const QString serverName = connectionName(QStringLiteral("server-loss"));
    const QString clientName = connectionName(QStringLiteral("client-loss"));
    auto server = QDBusConnection::connectToBus(bus.address(), serverName);
    auto client = QDBusConnection::connectToBus(bus.address(), clientName);
    QVERIFY(server.isConnected());
    QVERIFY(client.isConnected());
    QVERIFY(registerAllNames(server));

    ScreenSaverBackend backend;
    new FreedesktopScreenSaverAdaptor(&backend);
    new KdeScreenSaverAdaptor(&backend);
    QVERIFY(server.registerObject(QStringLiteral("/ScreenSaver"), &backend,
                                  QDBusConnection::ExportAdaptors));

    QtSessionLockTransport transport(client);
    SessionLockStateMonitor monitor(transport,
                                    QCoreApplication::applicationPid());
    QSignalSpy transportLostSpy(&transport,
                                &SessionLockTransport::transportLost);
    QVERIFY2(monitor.start(&error), qPrintable(error));
    QTRY_COMPARE_WITH_TIMEOUT(monitor.state(), LockState::Unlocked, 5'000);
    QVERIFY(monitor.contentMayBeShown());

    // Do not stop the monitor: terminating the isolated daemon must drive the
    // local Disconnected signal and revoke the previously granted authority.
    bus.stop();
    QTRY_COMPARE_WITH_TIMEOUT(transportLostSpy.size(), 1, 5'000);
    QTRY_COMPARE_WITH_TIMEOUT(monitor.state(), LockState::Unknown, 5'000);
    QVERIFY(!monitor.contentMayBeShown());
    QVERIFY(!monitor.isStarted());

    QDBusConnection::disconnectFromBus(clientName);
    QDBusConnection::disconnectFromBus(serverName);
    client = QDBusConnection(QStringLiteral("released-client-loss"));
    server = QDBusConnection(QStringLiteral("released-server-loss"));
}

QTEST_GUILESS_MAIN(QtSessionLockTransportTests)

#include "tst_qt_session_lock_transport.moc"
