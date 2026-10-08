// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/session_supervisor/src/replaced_activation_owner.h"
#include <QElapsedTimer>
#include <QFile>
#include <QDir>
#include <QThread>
#include <utility>
#include <QProcess>
#include <QTemporaryDir>
#include <QtDBus/QDBusConnectionInterface>
#include <QtTest>
#include <unistd.h>

using namespace QindaQt::SessionSupervisor;
namespace {
const QString Power = QStringLiteral("org.qindaqt.Power1");
const QString DeletedPower = QStringLiteral(QINDAQT_TEST_POWER_PATH " (deleted)");

class ModeledWitness final : public ReplacedOwnerProcessWitness {
public:
    mutable int reads = 0;
    int *signalCount = nullptr;
    QString scenario;
    QDBusConnection bus;
    explicit ModeledWitness(QDBusConnection connection) : bus(std::move(connection)) {}
    std::optional<ReplacedOwnerIdentity> identity() const override
    {
        ++reads;
        if (scenario == QStringLiteral("unreadable")) return std::nullopt;
        return ReplacedOwnerIdentity{
            static_cast<quint32>(getuid()) + (scenario == QStringLiteral("other-user") ? 1U : 0U),
            scenario == QStringLiteral("changed-lifetime") && reads > 1 ? 2U : 1U,
            scenario == QStringLiteral("healthy") ? QStringLiteral(QINDAQT_TEST_POWER_PATH)
            : scenario == QStringLiteral("wrong-executable") ? QStringLiteral("/other/service (deleted)")
            : DeletedPower};
    }
    bool terminate() override
    {
        ++*signalCount;
        if (scenario == QStringLiteral("signal-failure")) return false;
        if (scenario != QStringLiteral("unretired")) bus.unregisterService(Power);
        return true;
    }
};
}

class ReplacedPowerOwnerTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void closedNamesIncludeOnlyDemonstratedPowerExtension()
    {
        QCOMPARE(replacedActivationServiceNames(),
            QStringList({QStringLiteral("org.qindaqt.Settings1"), Power,
                         QStringLiteral("org.freedesktop.impl.portal.desktop.qindaqt")}));
        QVERIFY(!replacedActivationServiceNames().contains(QStringLiteral("org.qindaqt.Bluetooth1")));
        QVERIFY(!replacedActivationServiceNames().contains(QStringLiteral("org.qindaqt.Network1")));
    }
    void admission_data()
    {
        QTest::addColumn<QString>("scenario");
        QTest::addColumn<bool>("retired");
        for (const QString &row : {QStringLiteral("replaced"), QStringLiteral("healthy"),
              QStringLiteral("wrong-executable"), QStringLiteral("other-user"),
              QStringLiteral("unreadable"), QStringLiteral("changed-lifetime"),
              QStringLiteral("changed-owner"), QStringLiteral("changed-owner-pid"), QStringLiteral("signal-failure"),
              QStringLiteral("private")})
            QTest::newRow(qPrintable(row)) << row << (row == QStringLiteral("replaced"));
    }
    void admission()
    {
        QFETCH(QString, scenario);
        QFETCH(bool, retired);
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        QVERIFY(bus.registerService(Power));
        int signalCount = 0, factories = 0;
        bool pidMatched = false;
        bool changedPid = false;
        QProcess replacementChild;
        const QString replacementName = QStringLiteral("power-replacement-connection");
        auto replacement = QDBusConnection::connectToBus(QDBusConnection::SessionBus, replacementName);
        const auto factory = [&](qint64 pid, const QString &) {
            ++factories;
            pidMatched = pid == QCoreApplication::applicationPid();
            if (scenario == QStringLiteral("changed-owner")) {
                bus.unregisterService(Power);
                replacement.registerService(Power);
            }
            if (scenario == QStringLiteral("changed-owner-pid")) {
                bus.unregisterService(Power);
                replacementChild.start(QStringLiteral(QINDAQT_RESIDENT_SERVICE_REFRESH_PUBLISHER),
                    {QStringLiteral("--own"), Power});
                replacementChild.waitForStarted();
                QElapsedTimer wait; wait.start();
                while (wait.elapsed() < 3000
                       && !bus.interface()->isServiceRegistered(Power).value())
                    QThread::msleep(10);
                const auto pid = bus.interface()->servicePid(Power);
                changedPid = pid.isValid()
                    && pid.value() == static_cast<uint>(replacementChild.processId())
                    && pid.value() != static_cast<uint>(QCoreApplication::applicationPid());
            }
            auto witness = std::make_unique<ModeledWitness>(bus);
            witness->scenario = scenario; witness->signalCount = &signalCount;
            return witness;
        };
        const auto result = retireReplacedActivationOwners(bus, {Power},
            scenario == QStringLiteral("private") ? SessionActivationScope::Private
                                                  : SessionActivationScope::PhysicalDesktop,
            QStringLiteral("/fixture-only-proc"), factory);
        QCOMPARE(result, retired ? QStringList{Power} : QStringList{});
        QCOMPARE(signalCount, retired || scenario == QStringLiteral("signal-failure") ? 1 : 0);
        if (scenario == QStringLiteral("private")) QCOMPARE(factories, 0);
        else QVERIFY(pidMatched);
        if (scenario == QStringLiteral("changed-owner-pid")) {
            QVERIFY(changedPid);
            replacementChild.terminate(); QVERIFY(replacementChild.waitForFinished(3000));
        }
        if (retired) QVERIFY(!bus.interface()->isServiceRegistered(Power).value());
        bus.unregisterService(Power); replacement.unregisterService(Power);
        QDBusConnection::disconnectFromBus(replacementName);
    }
    void missingOwnerOrBusGrantsNoWitness()
    {
        auto bus = QDBusConnection::sessionBus();
        int factories = 0;
        const auto factory = [&](qint64, const QString &) -> std::unique_ptr<ReplacedOwnerProcessWitness> {
            ++factories; return {};
        };
        QVERIFY(retireReplacedActivationOwners(bus, {Power},
            SessionActivationScope::PhysicalDesktop, {}, factory).isEmpty());
        QVERIFY(retireReplacedActivationOwners(QDBusConnection(QStringLiteral("absent-power-bus")), {Power},
            SessionActivationScope::PhysicalDesktop, {}, factory).isEmpty());
        QCOMPARE(factories, 0);
    }
    void unretiredOwnerReturnIsBounded()
    {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.registerService(Power));
        int signalCount = 0;
        const auto factory = [&](qint64, const QString &) {
            auto witness = std::make_unique<ModeledWitness>(bus);
            witness->scenario = QStringLiteral("unretired"); witness->signalCount = &signalCount;
            return witness;
        };
        QElapsedTimer elapsed; elapsed.start();
        const auto result = retireReplacedActivationOwners(bus, {Power},
            SessionActivationScope::PhysicalDesktop, {}, factory);
        // This fixture establishes bounded return and retained owner only.
        // It does not execute the supervisor's full desktop startup caller.
        QVERIFY(result.isEmpty());
        QCOMPARE(signalCount, 1);
        QVERIFY(elapsed.elapsed() >= 1500); QVERIFY(elapsed.elapsed() < 3500);
        QVERIFY(bus.interface()->isServiceRegistered(Power).value());
        bus.unregisterService(Power);
    }
    void realHeldChildRetiresOnlyWhenReplaced_data()
    {
        QTest::addColumn<bool>("deleted");
        QTest::newRow("healthy-child") << false;
        QTest::newRow("replaced-child") << true;
    }
    void realHeldChildRetiresOnlyWhenReplaced()
    {
        QFETCH(bool, deleted);
        auto bus = QDBusConnection::sessionBus();
        QProcess child;
        child.start(QStringLiteral(QINDAQT_RESIDENT_SERVICE_REFRESH_PUBLISHER),
                    {QStringLiteral("--own"), Power});
        QVERIFY(child.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(bus.interface()->isServiceRegistered(Power).value(), 5000);
        QTemporaryDir proc; QVERIFY(proc.isValid());
        const QString directory = proc.filePath(QString::number(child.processId()));
        QVERIFY(QDir().mkpath(directory));
        QFile original(QStringLiteral("/proc/%1/stat").arg(child.processId()));
        QVERIFY(original.open(QIODevice::ReadOnly)); const QByteArray stat = original.readAll();
        QFile modeled(directory + QStringLiteral("/stat")); QVERIFY(modeled.open(QIODevice::WriteOnly));
        QCOMPARE(modeled.write(stat), static_cast<qint64>(stat.size())); modeled.close();
        QVERIFY(QFile::link(deleted ? DeletedPower : QStringLiteral(QINDAQT_TEST_POWER_PATH),
                            directory + QStringLiteral("/exe")));
        const auto result = retireReplacedActivationOwners(bus, {Power},
            SessionActivationScope::PhysicalDesktop, proc.path());
        QCOMPARE(result, deleted ? QStringList{Power} : QStringList{});
        if (deleted) {
            QVERIFY(child.waitForFinished(3000));
            QVERIFY(!bus.interface()->isServiceRegistered(Power).value());
        } else {
            QCOMPARE(child.state(), QProcess::Running);
            child.terminate(); QVERIFY(child.waitForFinished(3000));
        }
    }
};
QTEST_GUILESS_MAIN(ReplacedPowerOwnerTests)
#include "tst_replaced_power_owner.moc"
