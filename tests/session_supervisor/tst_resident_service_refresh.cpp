// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/session_supervisor/src/replaced_activation_owner.h"
#include "src/session_supervisor/src/resident_service_refresh.h"
#include "src/session_supervisor/src/resident_unit_staleness.h"

#include <QDir>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QProcess>
#include <QTimer>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusVariant>
#include <QtDBus/QDBusVirtualObject>
#include <QtDBus/QDBusConnectionInterface>
#include <QtTest>
#include <QTemporaryDir>

namespace {

// Fakes just enough of org.freedesktop.systemd1.Manager to observe
// RestartUnit calls without a real systemd user manager or real services.
// Every unit's MainPID defaults to this test process, which started before
// any login marker a test passes (stale); tests map units to fresher
// processes, to 0 (stopped) or to a missing unit.
class FakeUserManager final : public QDBusVirtualObject {
public:
    QStringList requestedUnits;
    QStringList requestedModes;
    QString failingUnit;
    QString delayedRetireUnit;
    int delayedRetireMilliseconds = 300;
    QHash<QString, quint32> mainPids;
    QStringList missingUnits;

    QString introspect(const QString &) const override { return {}; }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        if (message.member() == QStringLiteral("GetUnit")) {
            const QString unit = message.arguments().value(0).toString();
            if (missingUnits.contains(unit)) {
                connection.send(message.createErrorReply(
                    QStringLiteral("org.freedesktop.systemd1.NoSuchUnit"), unit));
                return true;
            }
            const QString path = QStringLiteral("/org/freedesktop/systemd1/unit/u%1")
                                     .arg(unitPaths.size());
            unitPaths.insert(path, unit);
            connection.send(message.createReply(QVariant::fromValue(QDBusObjectPath(path))));
            return true;
        }
        if (message.member() == QStringLiteral("Get") && unitPaths.contains(message.path())) {
            const QString unit = unitPaths.value(message.path());
            const quint32 pid = mainPids.value(
                unit, static_cast<quint32>(QCoreApplication::applicationPid()));
            connection.send(message.createReply(
                QVariant::fromValue(QDBusVariant(QVariant::fromValue(pid)))));
            return true;
        }
        if (message.member() != QStringLiteral("RestartUnit")) return false;
        const QString unit = message.arguments().value(0).toString();
        const QString mode = message.arguments().value(1).toString();
        requestedUnits.append(unit);
        requestedModes.append(mode);
        if (unit == delayedRetireUnit) {
            auto bus = connection;
            QTimer::singleShot(delayedRetireMilliseconds, this, [bus]() mutable {
                bus.unregisterService(QStringLiteral("org.qindaqt.Audio1"));
            });
        }
        if (unit == failingUnit) {
            connection.send(message.createErrorReply(
                QStringLiteral("org.freedesktop.systemd1.NoSuchUnit"),
                QStringLiteral("test-forced failure")));
            return true;
        }
        connection.send(message.createReply(
            QVariant::fromValue(QDBusObjectPath(QStringLiteral("/job/1")))));
        return true;
    }

private:
    QHash<QString, QString> unitPaths;
};

// A process started after this test process: its start is "this login".
struct LoginMarker {
    QProcess process;
    LoginMarker()
    {
        QTest::qWait(30); // at least one clock tick after the test process
        process.start(QStringLiteral("sleep"), {QStringLiteral("60")});
        process.waitForStarted();
    }
    ~LoginMarker()
    {
        process.kill();
        process.waitForFinished(3000);
    }
    QString pid() const { return QString::number(process.processId()); }
};

// Stands up a standalone dbus-daemon on an explicit unix:path and registers
// the fake user manager there, mirroring the systemd private control socket
// the user manager always exposes at $XDG_RUNTIME_DIR/systemd/private.
struct PrivateManagerBus {
    QProcess daemon;
    QString socket;
    QString connectionName;
    FakeUserManager manager;

    bool start(const QString &tag)
    {
        socket = QDir::temp().filePath(
            QStringLiteral("qindaqt-%1-%2.sock").arg(tag).arg(QCoreApplication::applicationPid()));
        connectionName = QStringLiteral("qindaqt-%1-test").arg(tag);
        daemon.start(QStringLiteral("dbus-daemon"),
                     {QStringLiteral("--session"), QStringLiteral("--nofork"),
                      QStringLiteral("--print-address=1"),
                      QStringLiteral("--address=unix:path=") + socket});
        return daemon.waitForStarted();
    }

    [[nodiscard]] bool bind()
    {
        QDBusConnection bus = QDBusConnection::connectToBus(
            QStringLiteral("unix:path=") + socket, connectionName);
        if (!bus.isConnected()) return false;
        if (!bus.registerService(QStringLiteral("org.freedesktop.systemd1"))) return false;
        return bus.registerVirtualObject(QStringLiteral("/org/freedesktop/systemd1"), &manager,
                                         QDBusConnection::SubPath);
    }

    ~PrivateManagerBus()
    {
        QDBusConnection::disconnectFromBus(connectionName);
        daemon.terminate();
        daemon.waitForFinished(3000);
    }
};

} // namespace

class ResidentServiceRefreshTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    // Owner, 2026-09-28: a PipeWire that stopped answering new clients
    // survived two logins and dictation recognized nothing. A daemon that
    // does not answer restarts the audio stack in order; one that answers,
    // or a Private session, is left alone.
    void unresponsivePipeWireRestartsTheAudioStack() {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        FakeUserManager manager;
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.systemd1")));
        QVERIFY(bus.registerVirtualObject(QStringLiteral("/org/freedesktop/systemd1"), &manager,
                                          QDBusConnection::SubPath));

        QProcess hung;
        hung.start(QStringLiteral(QINDAQT_RESIDENT_SERVICE_REFRESH_PUBLISHER),
                   {QStringLiteral("--socket"), QStringLiteral("/nonexistent"),
                    QStringLiteral("--pipewire-hung")});
        QVERIFY(hung.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(hung.state() == QProcess::NotRunning, 6000);
        QCOMPARE(hung.exitCode(), 0);
        QCOMPARE(manager.requestedUnits, QindaQt::SessionSupervisor::pipeWireStackUnits());
        for (const QString &mode : manager.requestedModes) {
            QCOMPARE(mode, QStringLiteral("replace"));
        }

        manager.requestedUnits.clear();
        manager.requestedModes.clear();
        QProcess answering;
        answering.start(QStringLiteral(QINDAQT_RESIDENT_SERVICE_REFRESH_PUBLISHER),
                        {QStringLiteral("--socket"), QStringLiteral("/nonexistent"),
                         QStringLiteral("--pipewire-answers")});
        QVERIFY(answering.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(answering.state() == QProcess::NotRunning, 6000);
        QCOMPARE(answering.exitCode(), 6);
        QVERIFY(manager.requestedUnits.isEmpty());

        QProcess privateSession;
        privateSession.start(QStringLiteral(QINDAQT_RESIDENT_SERVICE_REFRESH_PUBLISHER),
                             {QStringLiteral("--private"), QStringLiteral("--pipewire-hung")});
        QVERIFY(privateSession.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(privateSession.state() == QProcess::NotRunning, 6000);
        QCOMPARE(privateSession.exitCode(), 6);
        QVERIFY(manager.requestedUnits.isEmpty());

        bus.unregisterObject(QStringLiteral("/org/freedesktop/systemd1"));
        bus.unregisterService(QStringLiteral("org.freedesktop.systemd1"));
    }

    void replacedExecutableIsRecognizedFromTheRawLinkText() {
        using QindaQt::SessionSupervisor::executableWasReplaced;
        QVERIFY(executableWasReplaced(QStringLiteral("/usr/bin/qindaqt-settings-service (deleted)")));
        QVERIFY(!executableWasReplaced(QStringLiteral("/usr/bin/qindaqt-settings-service")));
        QVERIFY(!executableWasReplaced(QString{}));
        QVERIFY(QindaQt::SessionSupervisor::replacedActivationServiceNames()
                    .contains(QStringLiteral("org.qindaqt.Settings1")));
    }

    // Owner, 2026-09-28: after an update the previous login's Settings1 kept
    // answering with the old schema ("Settings snapshot is malformed or
    // regressed"). Only an owner whose executable was replaced is retired.
    void onlyAnOwnerWithAReplacedExecutableIsRetired() {
        using namespace QindaQt::SessionSupervisor;
        const QString name = QStringLiteral("org.qindaqt.ReplacedOwnerTest");
        auto bus = QDBusConnection::sessionBus();
        QProcess owner;
        owner.start(QStringLiteral(QINDAQT_RESIDENT_SERVICE_REFRESH_PUBLISHER),
                    {QStringLiteral("--own"), name});
        QVERIFY(owner.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(bus.interface()->isServiceRegistered(name).value(), 5000);
        QTemporaryDir proc;
        QVERIFY(proc.isValid());
        const QString processDirectory = proc.filePath(QString::number(owner.processId()));
        QVERIFY(QDir().mkpath(processDirectory));
        const QString exe = processDirectory + QStringLiteral("/exe");
        QFile actualStat(QStringLiteral("/proc/%1/stat").arg(owner.processId()));
        QVERIFY(actualStat.open(QIODevice::ReadOnly));
        QFile modeledStat(processDirectory + QStringLiteral("/stat"));
        QVERIFY(modeledStat.open(QIODevice::WriteOnly));
        const QByteArray processStat = actualStat.readAll();
        QCOMPARE(modeledStat.write(processStat), static_cast<qint64>(processStat.size()));
        modeledStat.close();


        // An up-to-date executable, or a Private session, is never touched.
        QVERIFY(QFile::link(QStringLiteral("/usr/bin/qindaqt-settings-service"), exe));
        QVERIFY(retireReplacedActivationOwners(
                    bus, {name}, SessionActivationScope::PhysicalDesktop, proc.path()).isEmpty());
        QVERIFY(retireReplacedActivationOwners(bus, {name}, SessionActivationScope::Private)
                    .isEmpty());
        QCOMPARE(owner.state(), QProcess::Running);

        QVERIFY(QFile::remove(exe));
        QVERIFY(QFile::link(QStringLiteral("/usr/bin/qindaqt-settings-service (deleted)"), exe));
        QCOMPARE(retireReplacedActivationOwners(
                     bus, {name}, SessionActivationScope::PhysicalDesktop, proc.path()),
                 QStringList{name});
        QVERIFY(owner.waitForFinished(3000));
        QVERIFY(!bus.interface()->isServiceRegistered(name).value());
    }

    void privateSessionCannotRestartSharedServices() {
        auto bus = QDBusConnection::sessionBus();
        FakeUserManager manager;
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.systemd1")));
        QVERIFY(bus.registerVirtualObject(QStringLiteral("/org/freedesktop/systemd1"), &manager,
                                          QDBusConnection::SubPath));
        QProcess publisher;
        publisher.start(QStringLiteral(QINDAQT_RESIDENT_SERVICE_REFRESH_PUBLISHER),
                        {QStringLiteral("--private"), QStringLiteral("xdg-desktop-portal.service")});
        QVERIFY(publisher.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(publisher.state() == QProcess::NotRunning, 6000);
        QCOMPARE(publisher.exitCode(), 0);
        QVERIFY(manager.requestedUnits.isEmpty());
        bus.unregisterObject(QStringLiteral("/org/freedesktop/systemd1"));
        bus.unregisterService(QStringLiteral("org.freedesktop.systemd1"));
    }

    // This is the regression the manual physical-session recovery on
    // 2026-09-06 stood in for: a service already resident from a prior
    // desktop keeps its stale Wayland socket after SetEnvironment runs, so
    // the session must explicitly restart it rather than only publish the
    // new environment for future activations.
    void restartsEachConfiguredResidentUnitIndependentlyOfEnvironmentPublication()
    {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        FakeUserManager manager;
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.systemd1")));
        QVERIFY(bus.registerVirtualObject(QStringLiteral("/org/freedesktop/systemd1"), &manager,
                                          QDBusConnection::SubPath));

        QProcess publisher;
        // No environment values are set here at all: this exercises only the
        // resident-refresh path, proving it is not merely a side effect of
        // republishing the activation environment.
        // Force the session-bus fallback lane: no private socket there, and
        // the fake manager owns the session-bus name. Production's native
        // lane is covered by the private-socket test below.
        LoginMarker login;
        publisher.start(QStringLiteral(QINDAQT_RESIDENT_SERVICE_REFRESH_PUBLISHER),
                        {QStringLiteral("--socket"), QStringLiteral("/nonexistent"),
                         QStringLiteral("--login-pid"), login.pid(),
                         QStringLiteral("qindaqt-test-resident-a.service"),
                         QStringLiteral("qindaqt-test-resident-b.service")});
        QVERIFY(publisher.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(publisher.state() == QProcess::NotRunning, 6000);
        QCOMPARE(publisher.exitCode(), 0);

        QCOMPARE(manager.requestedUnits,
                 QStringList({QStringLiteral("qindaqt-test-resident-a.service"),
                              QStringLiteral("qindaqt-test-resident-b.service")}));
        for (const QString &mode : manager.requestedModes) {
            QCOMPARE(mode, QStringLiteral("replace"));
        }
        QVERIFY(!publisher.readAllStandardError().contains("Could not restart"));

        bus.unregisterObject(QStringLiteral("/org/freedesktop/systemd1"));
        bus.unregisterService(QStringLiteral("org.freedesktop.systemd1"));
    }

    // Production regression (2026-09-13): on a private dbus-run-session
    // session bus, RestartUnit calls against org.freedesktop.systemd1 hit a
    // spurious second-user-manager activation and resident services were
    // never refreshed. The manager must be reached through its private
    // control socket instead.
    void restartsUnitsThroughTheSystemdPrivateSocketWhenItExists()
    {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        QVERIFY(!bus.interface()->isServiceRegistered(
            QStringLiteral("org.freedesktop.systemd1")));

        PrivateManagerBus managerBus;
        QVERIFY(managerBus.start(QStringLiteral("resident-refresh")));
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(managerBus.socket), 4000);
        QVERIFY(managerBus.bind());

        QProcess publisher;
        LoginMarker login;
        publisher.start(QStringLiteral(QINDAQT_RESIDENT_SERVICE_REFRESH_PUBLISHER),
                        {QStringLiteral("--private"), QStringLiteral("--socket"), managerBus.socket,
                         QStringLiteral("--login-pid"), login.pid(),
                         QStringLiteral("qindaqt-test-resident-a.service"),
                         QStringLiteral("qindaqt-test-resident-b.service")});
        QVERIFY(publisher.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(publisher.state() == QProcess::NotRunning, 6000);
        QCOMPARE(publisher.exitCode(), 0);

        QCOMPARE(managerBus.manager.requestedUnits,
                 QStringList({QStringLiteral("qindaqt-test-resident-a.service"),
                              QStringLiteral("qindaqt-test-resident-b.service")}));
        for (const QString &mode : managerBus.manager.requestedModes) {
            QCOMPARE(mode, QStringLiteral("replace"));
        }
        QVERIFY(!publisher.readAllStandardError().contains("Could not restart"));
    }

    // RestartUnit replies when its job is enqueued. Model the old owner
    // disappearing later, and require the helper to wait for that retirement.
    void audioRefreshWaitsUntilTheOldOwnerHasRetired()
    {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        FakeUserManager manager;
        manager.delayedRetireUnit = QStringLiteral("qindaqt-audio-service.service");
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.systemd1")));
        QVERIFY(bus.registerVirtualObject(QStringLiteral("/org/freedesktop/systemd1"), &manager,
                                          QDBusConnection::SubPath));
        QVERIFY(bus.registerService(QStringLiteral("org.qindaqt.Audio1")));
        QDBusMessage ownerQuery = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
            QStringLiteral("org.freedesktop.DBus"), QStringLiteral("GetNameOwner"));
        ownerQuery.setArguments({QStringLiteral("org.qindaqt.Audio1")});
        const auto oldOwnerReply = bus.call(ownerQuery);
        QVERIFY(oldOwnerReply.type() != QDBusMessage::ErrorMessage);
        const QString oldOwner = oldOwnerReply.arguments().constFirst().toString();

        QElapsedTimer elapsed;
        elapsed.start();
        QProcess publisher;
        LoginMarker login;
        publisher.start(QStringLiteral(QINDAQT_RESIDENT_SERVICE_REFRESH_PUBLISHER),
                        {QStringLiteral("--socket"), QStringLiteral("/nonexistent"),
                         QStringLiteral("--login-pid"), login.pid(),
                         QStringLiteral("qindaqt-audio-service.service")});
        QVERIFY(publisher.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(publisher.state() == QProcess::NotRunning, 6000);
        QCOMPARE(publisher.exitCode(), 0);
        QVERIFY(elapsed.elapsed() >= 250);
        QVERIFY(!bus.interface()->isServiceRegistered(QStringLiteral("org.qindaqt.Audio1")));
        QCOMPARE(manager.requestedUnits,
                 QStringList({QStringLiteral("qindaqt-audio-service.service")}));
        QVERIFY(oldOwner.startsWith(QLatin1Char(':')));

        bus.unregisterObject(QStringLiteral("/org/freedesktop/systemd1"));
        bus.unregisterService(QStringLiteral("org.freedesktop.systemd1"));
    }

    // A manager may accept a restart job without retiring an owner on a
    // private session bus. The wait stays bounded and reports that edge.
    void audioRefreshReportsAnOwnerThatOutlivesTheBound()
    {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        FakeUserManager manager;
        manager.delayedRetireUnit = QStringLiteral("qindaqt-audio-service.service");
        manager.delayedRetireMilliseconds = 2500;
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.systemd1")));
        QVERIFY(bus.registerVirtualObject(QStringLiteral("/org/freedesktop/systemd1"), &manager,
                                          QDBusConnection::SubPath));
        QVERIFY(bus.registerService(QStringLiteral("org.qindaqt.Audio1")));

        QElapsedTimer elapsed;
        elapsed.start();
        QProcess publisher;
        LoginMarker login;
        publisher.start(QStringLiteral(QINDAQT_RESIDENT_SERVICE_REFRESH_PUBLISHER),
                        {QStringLiteral("--socket"), QStringLiteral("/nonexistent"),
                         QStringLiteral("--login-pid"), login.pid(),
                         QStringLiteral("qindaqt-audio-service.service")});
        QVERIFY(publisher.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(publisher.state() == QProcess::NotRunning, 6000);
        QCOMPARE(publisher.exitCode(), 3);
        QVERIFY(elapsed.elapsed() >= 1500);
        QVERIFY(bus.interface()->isServiceRegistered(QStringLiteral("org.qindaqt.Audio1")));

        bus.unregisterService(QStringLiteral("org.qindaqt.Audio1"));
        bus.unregisterObject(QStringLiteral("/org/freedesktop/systemd1"));
        bus.unregisterService(QStringLiteral("org.freedesktop.systemd1"));
    }

    void oneFailingUnitDoesNotStopTheRemainingRefresh()
    {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        FakeUserManager manager;
        manager.failingUnit = QStringLiteral("qindaqt-test-resident-a.service");
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.systemd1")));
        QVERIFY(bus.registerVirtualObject(QStringLiteral("/org/freedesktop/systemd1"), &manager,
                                          QDBusConnection::SubPath));

        QProcess publisher;
        LoginMarker login;
        publisher.start(QStringLiteral(QINDAQT_RESIDENT_SERVICE_REFRESH_PUBLISHER),
                        {QStringLiteral("--socket"), QStringLiteral("/nonexistent"),
                         QStringLiteral("--login-pid"), login.pid(),
                         QStringLiteral("qindaqt-test-resident-a.service"),
                         QStringLiteral("qindaqt-test-resident-b.service")});
        QVERIFY(publisher.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(publisher.state() == QProcess::NotRunning, 6000);
        // Best-effort: the helper process still exits cleanly even though one
        // restart failed.
        QCOMPARE(publisher.exitCode(), 0);

        QCOMPARE(manager.requestedUnits,
                 QStringList({QStringLiteral("qindaqt-test-resident-a.service"),
                              QStringLiteral("qindaqt-test-resident-b.service")}));
        QVERIFY(publisher.readAllStandardError().contains("Could not restart"));

        bus.unregisterObject(QStringLiteral("/org/freedesktop/systemd1"));
        bus.unregisterService(QStringLiteral("org.freedesktop.systemd1"));
    }

    // The fixed list is reviewed per module: session-bound Wayland/routing
    // users plus Audio1's demonstrated stale-package owner failure. The
    // other D-Bus-only Settings services remain outside this list.
    void residentUnitListNamesOnlyReviewedResidentServices()
    {
        // ADR-0379 removed the retired Plasma portal backend.
        const QStringList units = QindaQt::SessionSupervisor::residentServiceRefreshUnits();
        QCOMPARE(units, QStringList({QStringLiteral("qindaqt-audio-service.service"),
                                     QStringLiteral("qindaqt-clipboard-host.service"),
                                     QStringLiteral("qindaqt-display-service.service"),
                                     QStringLiteral("xdg-desktop-portal.service")}));
    }

    void stalenessRuleClassifiesInstances()
    {
        using namespace QindaQt::SessionSupervisor;
        const auto state = [](quint32 pid, std::optional<quint64> ticks, bool replaced,
                              std::optional<quint64> login) {
            ResidentUnitInstance instance;
            instance.unit = QStringLiteral("u.service");
            instance.mainPid = pid;
            instance.startTicks = ticks;
            instance.executableReplaced = replaced;
            return classifyResidentUnit(instance, login);
        };
        QCOMPARE(state(0, 5, true, 10), ResidentUnitState::NotRunning);
        QCOMPARE(state(42, 5, false, 10), ResidentUnitState::PredatesLogin);
        QCOMPARE(state(42, 10, false, 10), ResidentUnitState::StartedThisLogin);
        QCOMPARE(state(42, 11, false, 10), ResidentUnitState::StartedThisLogin);
        QCOMPARE(state(42, 11, true, 10), ResidentUnitState::ExecutableReplaced);
        QCOMPARE(state(42, std::nullopt, false, 10), ResidentUnitState::Unobservable);
        QCOMPARE(state(42, 5, false, std::nullopt), ResidentUnitState::Unobservable);
        QVERIFY(residentUnitIsStale(ResidentUnitState::PredatesLogin));
        QVERIFY(residentUnitIsStale(ResidentUnitState::ExecutableReplaced));
        QVERIFY(!residentUnitIsStale(ResidentUnitState::StartedThisLogin));
        QVERIFY(!residentUnitIsStale(ResidentUnitState::NotRunning));
        QVERIFY(!residentUnitIsStale(ResidentUnitState::Unobservable));
        const auto self = processStartTicks(QCoreApplication::applicationPid());
        QVERIFY(self.has_value());
        QVERIFY(!processExecutableReplaced(QCoreApplication::applicationPid()));
    }

    // Owner, 2026-10-16: the login refresh restarted xdg-desktop-portal 3.5 s
    // after it started, killing the screen-capture broker. Only an instance
    // that predates this login is restarted.
    void restartsOnlyUnitsWhoseInstancePredatesThisLogin()
    {
        auto bus = QDBusConnection::sessionBus();
        FakeUserManager manager;
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.systemd1")));
        QVERIFY(bus.registerVirtualObject(QStringLiteral("/org/freedesktop/systemd1"), &manager,
                                          QDBusConnection::SubPath));
        LoginMarker login;
        QProcess fresh;
        fresh.start(QStringLiteral("sleep"), {QStringLiteral("60")});
        QVERIFY(fresh.waitForStarted());
        manager.mainPids.insert(QStringLiteral("fresh.service"),
                                static_cast<quint32>(fresh.processId()));
        manager.mainPids.insert(QStringLiteral("stopped.service"), 0);
        manager.missingUnits.append(QStringLiteral("missing.service"));
        QProcess publisher;
        publisher.start(QStringLiteral(QINDAQT_RESIDENT_SERVICE_REFRESH_PUBLISHER),
                        {QStringLiteral("--socket"), QStringLiteral("/nonexistent"),
                         QStringLiteral("--login-pid"), login.pid(),
                         QStringLiteral("stale.service"), QStringLiteral("fresh.service"),
                         QStringLiteral("stopped.service"), QStringLiteral("missing.service")});
        QVERIFY(publisher.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(publisher.state() == QProcess::NotRunning, 6000);
        QCOMPARE(publisher.exitCode(), 0);
        QCOMPARE(manager.requestedUnits, QStringList{QStringLiteral("stale.service")});
        const QByteArray log = publisher.readAllStandardError();
        QVERIFY(log.contains("Kept resident session unit fresh.service: started in this login"));
        QVERIFY(log.contains("Kept resident session unit stopped.service: not running"));
        QVERIFY(log.contains("Restarting resident session unit stale.service: predates this login"));
        // Without a login reference only replaced executables would restart.
        manager.requestedUnits.clear();
        QProcess unanchored;
        unanchored.start(QStringLiteral(QINDAQT_RESIDENT_SERVICE_REFRESH_PUBLISHER),
                         {QStringLiteral("--socket"), QStringLiteral("/nonexistent"),
                          QStringLiteral("stale.service")});
        QVERIFY(unanchored.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(unanchored.state() == QProcess::NotRunning, 6000);
        QVERIFY(manager.requestedUnits.isEmpty());
        fresh.kill();
        fresh.waitForFinished(3000);
        bus.unregisterObject(QStringLiteral("/org/freedesktop/systemd1"), QDBusConnection::UnregisterTree);
        bus.unregisterService(QStringLiteral("org.freedesktop.systemd1"));
    }
};
QTEST_GUILESS_MAIN(ResidentServiceRefreshTests)
#include "tst_resident_service_refresh.moc"
