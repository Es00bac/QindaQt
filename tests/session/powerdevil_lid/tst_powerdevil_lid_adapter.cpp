// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/session/powerdevil_lid/powerdevil_lid_adapter.h>

#include <KConfig>
#include <KConfigGroup>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusError>
#include <QtTest/QSignalSpy>
#include <QtTest/QtTest>

namespace {

constexpr auto PowerDevilService = "org.kde.Solid.PowerManagement";
constexpr auto PowerDevilPath = "/org/kde/Solid/PowerManagement";
constexpr auto PowerDevilInterface = "org.kde.Solid.PowerManagement";

class FakePowerDevil final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement")

public:
    int refreshCount = 0;
    bool failRefresh = false;

public Q_SLOTS:
    void refreshStatus()
    {
        if (failRefresh) {
            sendErrorReply(QDBusError::Failed,
                           QStringLiteral("test-refresh-failed"));
            return;
        }
        ++refreshCount;
        Q_EMIT refreshed();
    }

Q_SIGNALS:
    void refreshed();
};

bool registerPowerDevil(QDBusConnection &bus, FakePowerDevil &service)
{
    return bus.registerService(QString::fromLatin1(PowerDevilService))
        && bus.registerObject(QString::fromLatin1(PowerDevilPath), &service,
                              QDBusConnection::ExportAllSlots);
}

void unregisterPowerDevil(QDBusConnection &bus)
{
    bus.unregisterObject(QString::fromLatin1(PowerDevilPath));
    bus.unregisterService(QString::fromLatin1(PowerDevilService));
}

QVariant readEntry(const QString &path, const QString &profile,
                   const QString &group, const QString &key)
{
    KConfig config(path, KConfig::SimpleConfig);
    config.reparseConfiguration();
    return config.group(profile).group(group).readEntry(key);
}

} // namespace

class PowerDevilLidAdapterTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void writesProfileKeysAndPreservesUnrelatedKeys();
    void secondApplyUpdatesValuesAndInhibitFlag();
    void absentOwnerRejectsWithoutWritingAndRecovers();
    void unsupportedActionValuesRejectedWithoutWriting();
    void reportsRefreshFailureAfterConfigSync();
    void applyWhileRefreshIsInFlightIsRejectedAsBusy();

private:
    QTemporaryDir m_configRoot;
    QString m_configFile;
};

void PowerDevilLidAdapterTest::initTestCase()
{
    QVERIFY(m_configRoot.isValid());
    qputenv("XDG_CONFIG_HOME", m_configRoot.path().toUtf8());
    m_configFile = QDir(m_configRoot.path()).filePath(QStringLiteral("powerdevilrc"));
}

void PowerDevilLidAdapterTest::cleanup()
{
    QFile::remove(m_configFile);
}

void PowerDevilLidAdapterTest::writesProfileKeysAndPreservesUnrelatedKeys()
{
    KConfig config(m_configFile, KConfig::SimpleConfig);
    config.group(QStringLiteral("AC")).group(QStringLiteral("SuspendAndShutdown"))
        .writeEntry(QStringLiteral("SleepMode"), QStringLiteral("keep-me"));
    config.group(QStringLiteral("AC")).group(QStringLiteral("Display"))
        .writeEntry(QStringLiteral("UnrelatedKey"), QStringLiteral("keep-me"));
    config.sync();

    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePowerDevil fake;
    QVERIFY(registerPowerDevil(bus, fake));
    QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter adapter(bus);
    adapter.start();
    QTRY_VERIFY(adapter.available());

    QSignalSpy finished(&adapter,
                        &QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter::applyFinished);
    QVERIFY(adapter.apply(QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter::Sleep,
                          true,
                          QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter::LockScreen));
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toBool(), true);
    QCOMPARE(fake.refreshCount, 1);

    for (const QString &profile : {QStringLiteral("AC"), QStringLiteral("Battery"),
                                   QStringLiteral("LowBattery")}) {
        QCOMPARE(readEntry(m_configFile, profile,
                           QStringLiteral("SuspendAndShutdown"), QStringLiteral("LidAction")),
                 QVariant(1u));
        QCOMPARE(readEntry(m_configFile, profile,
                           QStringLiteral("SuspendAndShutdown"),
                           QStringLiteral("InhibitLidActionWhenExternalMonitorPresent")),
                 QVariant(true));
        QCOMPARE(readEntry(m_configFile, profile,
                           QStringLiteral("SuspendAndShutdown"),
                           QStringLiteral("PowerButtonAction")),
                 QVariant(32u));
    }
    // SleepMode is not owned by this adapter and must keep its value.
    QCOMPARE(readEntry(m_configFile, QStringLiteral("AC"),
                       QStringLiteral("SuspendAndShutdown"), QStringLiteral("SleepMode")),
             QVariant(QStringLiteral("keep-me")));
    QCOMPARE(readEntry(m_configFile, QStringLiteral("AC"), QStringLiteral("Display"),
                       QStringLiteral("UnrelatedKey")),
             QVariant(QStringLiteral("keep-me")));
    unregisterPowerDevil(bus);
}

void PowerDevilLidAdapterTest::secondApplyUpdatesValuesAndInhibitFlag()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePowerDevil fake;
    QVERIFY(registerPowerDevil(bus, fake));
    QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter adapter(bus);
    adapter.start();
    QTRY_VERIFY(adapter.available());

    QSignalSpy finished(&adapter,
                        &QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter::applyFinished);
    QVERIFY(adapter.apply(QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter::Hibernate,
                          false,
                          QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter::TurnOffScreen));
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toBool(), true);
    QCOMPARE(fake.refreshCount, 1);
    for (const QString &profile : {QStringLiteral("AC"), QStringLiteral("Battery"),
                                   QStringLiteral("LowBattery")}) {
        QCOMPARE(readEntry(m_configFile, profile,
                           QStringLiteral("SuspendAndShutdown"), QStringLiteral("LidAction")),
                 QVariant(2u));
        QCOMPARE(readEntry(m_configFile, profile,
                           QStringLiteral("SuspendAndShutdown"),
                           QStringLiteral("InhibitLidActionWhenExternalMonitorPresent")),
                 QVariant(false));
        QCOMPARE(readEntry(m_configFile, profile,
                           QStringLiteral("SuspendAndShutdown"),
                           QStringLiteral("PowerButtonAction")),
                 QVariant(64u));
    }
    unregisterPowerDevil(bus);
}

void PowerDevilLidAdapterTest::absentOwnerRejectsWithoutWritingAndRecovers()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter adapter(bus);
    adapter.start();
    QTRY_VERIFY(!adapter.available());

    QSignalSpy finished(&adapter,
                        &QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter::applyFinished);
    QVERIFY(!adapter.apply(QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter::Sleep,
                           true,
                           QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter::ShutDown));
    QCOMPARE(adapter.error(), QStringLiteral("powerdevil-unavailable"));
    QCOMPARE(finished.count(), 1);
    QVERIFY(!QFile::exists(m_configFile));

    FakePowerDevil fake;
    QVERIFY(registerPowerDevil(bus, fake));
    QTRY_VERIFY(adapter.available());
    QTRY_COMPARE(fake.refreshCount, 1);
    QVERIFY(adapter.error().isEmpty());

    QVERIFY(adapter.apply(QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter::DoNothing,
                          true,
                          QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter::DoNothing));
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(finished.at(1).at(0).toBool(), true);
    QCOMPARE(readEntry(m_configFile, QStringLiteral("LowBattery"),
                       QStringLiteral("SuspendAndShutdown"), QStringLiteral("LidAction")),
             QVariant(0u));
    unregisterPowerDevil(bus);
}

void PowerDevilLidAdapterTest::unsupportedActionValuesRejectedWithoutWriting()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePowerDevil fake;
    QVERIFY(registerPowerDevil(bus, fake));
    QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter adapter(bus);
    adapter.start();
    QTRY_VERIFY(adapter.available());

    using QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter;
    QSignalSpy finished(&adapter, &PowerDevilLidAdapter::applyFinished);
    // 3 is not an enum value; 16 (PromptLogoutDialog) and 128
    // (ToggleScreenOnOff) exist upstream but are outside the offered set;
    // 129 is foreign entirely.
    QVERIFY(!adapter.apply(3, true, PowerDevilLidAdapter::Sleep));
    QCOMPARE(adapter.error(), QStringLiteral("lid-action-unsupported"));
    QVERIFY(!adapter.apply(PowerDevilLidAdapter::Sleep, true, 16));
    QCOMPARE(adapter.error(), QStringLiteral("power-button-action-unsupported"));
    QVERIFY(!adapter.apply(128, true, 129));
    QCOMPARE(finished.count(), 3);
    QCOMPARE(fake.refreshCount, 0);
    QVERIFY(!QFile::exists(m_configFile));
    unregisterPowerDevil(bus);
}

void PowerDevilLidAdapterTest::reportsRefreshFailureAfterConfigSync()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePowerDevil fake;
    fake.failRefresh = true;
    QVERIFY(registerPowerDevil(bus, fake));
    QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter adapter(bus);
    adapter.start();
    QTRY_VERIFY(adapter.available());

    QSignalSpy finished(&adapter,
                        &QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter::applyFinished);
    QVERIFY(adapter.apply(QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter::Sleep,
                          true,
                          QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter::Sleep));
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toBool(), false);
    QVERIFY(adapter.error().startsWith(QStringLiteral("powerdevil-refresh-failed:")));
    // The synced config is not rolled back when the daemon reload fails.
    QCOMPARE(readEntry(m_configFile, QStringLiteral("AC"),
                       QStringLiteral("SuspendAndShutdown"), QStringLiteral("LidAction")),
             QVariant(1u));
    unregisterPowerDevil(bus);
}

void PowerDevilLidAdapterTest::applyWhileRefreshIsInFlightIsRejectedAsBusy()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePowerDevil fake;
    QVERIFY(registerPowerDevil(bus, fake));
    QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter adapter(bus);
    adapter.start();
    QTRY_VERIFY(adapter.available());

    using QindaQt::Session::PowerDevilLid::PowerDevilLidAdapter;
    QSignalSpy finished(&adapter, &PowerDevilLidAdapter::applyFinished);
    // No event loop spin between the two calls: the refresh reply of the
    // first apply cannot have arrived, so the second must fail closed.
    QVERIFY(adapter.apply(PowerDevilLidAdapter::Sleep, true,
                          PowerDevilLidAdapter::LockScreen));
    QVERIFY(!adapter.apply(PowerDevilLidAdapter::DoNothing, true,
                           PowerDevilLidAdapter::DoNothing));
    QCOMPARE(adapter.error(), QStringLiteral("powerdevil-apply-busy"));
    QCOMPARE(finished.count(), 1);
    QTRY_COMPARE(finished.count(), 2);
    // The busy rejection is emitted synchronously before the refresh reply.
    QCOMPARE(finished.at(0).at(0).toBool(), false);
    QCOMPARE(finished.at(1).at(0).toBool(), true);
    QCOMPARE(fake.refreshCount, 1);
    unregisterPowerDevil(bus);
}

QTEST_MAIN(PowerDevilLidAdapterTest)
#include "tst_powerdevil_lid_adapter.moc"
