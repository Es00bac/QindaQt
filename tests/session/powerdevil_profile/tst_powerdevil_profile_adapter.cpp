// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/session/powerdevil_profile/powerdevil_profile_adapter.h>

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

bool hasEntry(const QString &path, const QString &profile, const QString &group,
             const QString &key)
{
    KConfig config(path, KConfig::SimpleConfig);
    config.reparseConfiguration();
    return config.group(profile).group(group).hasKey(key);
}

} // namespace

class PowerDevilProfileAdapterTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void writesDistinctPerSourceProfilesAndPreservesUnrelatedKeys();
    void emptyIdDeletesTheKeyInsteadOfWritingEmpty();
    void absentOwnerRejectsWithoutWritingAndRecovers();
    void newlineInAnyIdIsRejectedWithoutWriting();
    void reportsRefreshFailureAfterConfigSync();
    void applyWhileRefreshIsInFlightIsRejectedAsBusy();

private:
    QTemporaryDir m_configRoot;
    QString m_configFile;
};

void PowerDevilProfileAdapterTest::initTestCase()
{
    QVERIFY(m_configRoot.isValid());
    qputenv("XDG_CONFIG_HOME", m_configRoot.path().toUtf8());
    m_configFile = QDir(m_configRoot.path()).filePath(QStringLiteral("powerdevilrc"));
}

void PowerDevilProfileAdapterTest::cleanup()
{
    QFile::remove(m_configFile);
}

void PowerDevilProfileAdapterTest::
    writesDistinctPerSourceProfilesAndPreservesUnrelatedKeys()
{
    KConfig config(m_configFile, KConfig::SimpleConfig);
    config.group(QStringLiteral("AC")).group(QStringLiteral("SuspendAndShutdown"))
        .writeEntry(QStringLiteral("LidAction"), 1u);
    config.group(QStringLiteral("AC")).group(QStringLiteral("Display"))
        .writeEntry(QStringLiteral("UnrelatedKey"), QStringLiteral("keep-me"));
    config.sync();

    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePowerDevil fake;
    QVERIFY(registerPowerDevil(bus, fake));
    QindaQt::Session::PowerDevilProfile::PowerDevilProfileAdapter adapter(bus);
    adapter.start();
    QTRY_VERIFY(adapter.available());

    QSignalSpy finished(
        &adapter,
        &QindaQt::Session::PowerDevilProfile::PowerDevilProfileAdapter::applyFinished);
    // Distinct per-source values -- the whole point of this adapter, unlike
    // the lid adapter's one shared value across all three profiles.
    QVERIFY(adapter.apply(QStringLiteral("performance"), QStringLiteral("balanced"),
                          QStringLiteral("power-saver")));
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toBool(), true);
    QCOMPARE(fake.refreshCount, 1);

    QCOMPARE(readEntry(m_configFile, QStringLiteral("AC"), QStringLiteral("Performance"),
                       QStringLiteral("PowerProfile")),
             QVariant(QStringLiteral("performance")));
    QCOMPARE(readEntry(m_configFile, QStringLiteral("Battery"),
                       QStringLiteral("Performance"), QStringLiteral("PowerProfile")),
             QVariant(QStringLiteral("balanced")));
    QCOMPARE(readEntry(m_configFile, QStringLiteral("LowBattery"),
                       QStringLiteral("Performance"), QStringLiteral("PowerProfile")),
             QVariant(QStringLiteral("power-saver")));
    // Neither this adapter's own unrelated AC group nor another route's key
    // is disturbed.
    QCOMPARE(readEntry(m_configFile, QStringLiteral("AC"),
                       QStringLiteral("SuspendAndShutdown"), QStringLiteral("LidAction")),
             QVariant(1u));
    QCOMPARE(readEntry(m_configFile, QStringLiteral("AC"), QStringLiteral("Display"),
                       QStringLiteral("UnrelatedKey")),
             QVariant(QStringLiteral("keep-me")));
    QCOMPARE(adapter.acProfileId(), QStringLiteral("performance"));
    QCOMPARE(adapter.batteryProfileId(), QStringLiteral("balanced"));
    QCOMPARE(adapter.lowBatteryProfileId(), QStringLiteral("power-saver"));
    unregisterPowerDevil(bus);
}

void PowerDevilProfileAdapterTest::emptyIdDeletesTheKeyInsteadOfWritingEmpty()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePowerDevil fake;
    QVERIFY(registerPowerDevil(bus, fake));
    QindaQt::Session::PowerDevilProfile::PowerDevilProfileAdapter adapter(bus);
    adapter.start();
    QTRY_VERIFY(adapter.available());

    QSignalSpy finished(
        &adapter,
        &QindaQt::Session::PowerDevilProfile::PowerDevilProfileAdapter::applyFinished);
    QVERIFY(adapter.apply(QStringLiteral("performance"), QString(), QString()));
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toBool(), true);

    QVERIFY(hasEntry(m_configFile, QStringLiteral("AC"), QStringLiteral("Performance"),
                     QStringLiteral("PowerProfile")));
    QVERIFY(!hasEntry(m_configFile, QStringLiteral("Battery"),
                      QStringLiteral("Performance"), QStringLiteral("PowerProfile")));
    QVERIFY(!hasEntry(m_configFile, QStringLiteral("LowBattery"),
                      QStringLiteral("Performance"), QStringLiteral("PowerProfile")));
    QVERIFY(adapter.batteryProfileId().isEmpty());
    QVERIFY(adapter.lowBatteryProfileId().isEmpty());

    // Re-enabling one that was previously cleared writes it back.
    QVERIFY(adapter.apply(QStringLiteral("performance"), QStringLiteral("balanced"),
                          QString()));
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(readEntry(m_configFile, QStringLiteral("Battery"),
                       QStringLiteral("Performance"), QStringLiteral("PowerProfile")),
             QVariant(QStringLiteral("balanced")));
    unregisterPowerDevil(bus);
}

void PowerDevilProfileAdapterTest::absentOwnerRejectsWithoutWritingAndRecovers()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QindaQt::Session::PowerDevilProfile::PowerDevilProfileAdapter adapter(bus);
    adapter.start();
    QTRY_VERIFY(!adapter.available());

    QSignalSpy finished(
        &adapter,
        &QindaQt::Session::PowerDevilProfile::PowerDevilProfileAdapter::applyFinished);
    QVERIFY(!adapter.apply(QStringLiteral("performance"), QStringLiteral("balanced"),
                           QStringLiteral("power-saver")));
    QCOMPARE(adapter.error(), QStringLiteral("powerdevil-unavailable"));
    QCOMPARE(finished.count(), 1);
    QVERIFY(!QFile::exists(m_configFile));

    FakePowerDevil fake;
    QVERIFY(registerPowerDevil(bus, fake));
    QTRY_VERIFY(adapter.available());
    QTRY_COMPARE(fake.refreshCount, 1);
    QVERIFY(adapter.error().isEmpty());

    QVERIFY(adapter.apply(QStringLiteral("balanced"), QStringLiteral("balanced"),
                          QStringLiteral("power-saver")));
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(finished.at(1).at(0).toBool(), true);
    unregisterPowerDevil(bus);
}

void PowerDevilProfileAdapterTest::newlineInAnyIdIsRejectedWithoutWriting()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePowerDevil fake;
    QVERIFY(registerPowerDevil(bus, fake));
    QindaQt::Session::PowerDevilProfile::PowerDevilProfileAdapter adapter(bus);
    adapter.start();
    QTRY_VERIFY(adapter.available());

    QSignalSpy finished(
        &adapter,
        &QindaQt::Session::PowerDevilProfile::PowerDevilProfileAdapter::applyFinished);
    QVERIFY(!adapter.apply(QStringLiteral("performance\nHidden=true"), QString(),
                           QString()));
    QCOMPARE(adapter.error(), QStringLiteral("power-profile-id-invalid"));
    QCOMPARE(finished.count(), 1);
    QCOMPARE(fake.refreshCount, 0);
    QVERIFY(!QFile::exists(m_configFile));
    unregisterPowerDevil(bus);
}

void PowerDevilProfileAdapterTest::reportsRefreshFailureAfterConfigSync()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePowerDevil fake;
    fake.failRefresh = true;
    QVERIFY(registerPowerDevil(bus, fake));
    QindaQt::Session::PowerDevilProfile::PowerDevilProfileAdapter adapter(bus);
    adapter.start();
    QTRY_VERIFY(adapter.available());

    QSignalSpy finished(
        &adapter,
        &QindaQt::Session::PowerDevilProfile::PowerDevilProfileAdapter::applyFinished);
    QVERIFY(adapter.apply(QStringLiteral("performance"), QStringLiteral("performance"),
                          QStringLiteral("performance")));
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toBool(), false);
    QVERIFY(adapter.error().startsWith(QStringLiteral("powerdevil-refresh-failed:")));
    // The synced config is not rolled back when the daemon reload fails.
    QCOMPARE(readEntry(m_configFile, QStringLiteral("AC"), QStringLiteral("Performance"),
                       QStringLiteral("PowerProfile")),
             QVariant(QStringLiteral("performance")));
    unregisterPowerDevil(bus);
}

void PowerDevilProfileAdapterTest::applyWhileRefreshIsInFlightIsRejectedAsBusy()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePowerDevil fake;
    QVERIFY(registerPowerDevil(bus, fake));
    QindaQt::Session::PowerDevilProfile::PowerDevilProfileAdapter adapter(bus);
    adapter.start();
    QTRY_VERIFY(adapter.available());

    QSignalSpy finished(
        &adapter,
        &QindaQt::Session::PowerDevilProfile::PowerDevilProfileAdapter::applyFinished);
    QVERIFY(adapter.apply(QStringLiteral("performance"), QString(), QString()));
    QVERIFY(!adapter.apply(QStringLiteral("balanced"), QString(), QString()));
    QCOMPARE(adapter.error(), QStringLiteral("powerdevil-apply-busy"));
    QCOMPARE(finished.count(), 1);
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(finished.at(0).at(0).toBool(), false);
    QCOMPARE(finished.at(1).at(0).toBool(), true);
    QCOMPARE(fake.refreshCount, 1);
    unregisterPowerDevil(bus);
}

QTEST_MAIN(PowerDevilProfileAdapterTest)
#include "tst_powerdevil_profile_adapter.moc"
