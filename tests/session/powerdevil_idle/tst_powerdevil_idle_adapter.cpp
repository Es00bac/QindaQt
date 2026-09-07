// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/session/powerdevil_idle/powerdevil_idle_adapter.h>

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

QString configPath(const QString &root)
{
    return QDir(root).filePath(QStringLiteral("powerdevilrc"));
}

QVariant readDisplayEntry(const QString &path, const QString &profile,
                          const QString &key)
{
    KConfig config(path, KConfig::SimpleConfig);
    config.reparseConfiguration();
    return config.group(profile).group(QStringLiteral("Display")).readEntry(key);
}

} // namespace

class PowerDevilIdleAdapterTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void writesPowerDevilSecondsAndPreservesUnrelatedKeys();
    void disabledSettingRetainsConfiguredTimeout();
    void reportsAbsentOwnerAndRecoversAfterRestart();
    void rejectsInvalidTimeoutWithoutWriting();
    void reportsRefreshFailureAfterConfigSync();
    void appliesWhenOwnerBecomesAvailable();
    void refreshesAfterOwnerReplacement();

private:
    QTemporaryDir m_configRoot;
    QString m_configFile;
};

void PowerDevilIdleAdapterTest::initTestCase()
{
    QVERIFY(m_configRoot.isValid());
    qputenv("XDG_CONFIG_HOME", m_configRoot.path().toUtf8());
    m_configFile = configPath(m_configRoot.path());
}

void PowerDevilIdleAdapterTest::cleanup()
{
    QFile::remove(m_configFile);
}

void PowerDevilIdleAdapterTest::writesPowerDevilSecondsAndPreservesUnrelatedKeys()
{
    KConfig config(m_configFile, KConfig::SimpleConfig);
    config.group(QStringLiteral("AC")).group(QStringLiteral("Display"))
        .writeEntry(QStringLiteral("UnrelatedKey"), QStringLiteral("keep-me"));
    config.sync();

    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePowerDevil fake;
    QVERIFY(registerPowerDevil(bus, fake));
    QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter adapter(bus);
    adapter.start();
    QTRY_VERIFY(adapter.available());

    QSignalSpy finished(&adapter,
                       &QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter::applyFinished);
    QVERIFY(adapter.apply(true, 7));
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toBool(), true);
    QCOMPARE(fake.refreshCount, 1);

    for (const QString &profile : {QStringLiteral("AC"), QStringLiteral("Battery"),
                                   QStringLiteral("LowBattery")}) {
        QCOMPARE(readDisplayEntry(m_configFile, profile,
                                  QStringLiteral("TurnOffDisplayWhenIdle")), QVariant(true));
        QCOMPARE(readDisplayEntry(m_configFile, profile,
                                  QStringLiteral("TurnOffDisplayIdleTimeoutSec")), QVariant(420));
    }
    QCOMPARE(readDisplayEntry(m_configFile, QStringLiteral("AC"),
                              QStringLiteral("UnrelatedKey")), QVariant(QStringLiteral("keep-me")));
    unregisterPowerDevil(bus);
}

void PowerDevilIdleAdapterTest::disabledSettingRetainsConfiguredTimeout()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePowerDevil fake;
    QVERIFY(registerPowerDevil(bus, fake));
    QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter adapter(bus);
    adapter.start();
    QTRY_VERIFY(adapter.available());

    QSignalSpy finished(&adapter,
                       &QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter::applyFinished);
    QVERIFY(adapter.apply(false, 9));
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toBool(), true);
    QCOMPARE(readDisplayEntry(m_configFile, QStringLiteral("AC"),
                              QStringLiteral("TurnOffDisplayWhenIdle")), QVariant(false));
    QCOMPARE(readDisplayEntry(m_configFile, QStringLiteral("AC"),
                              QStringLiteral("TurnOffDisplayIdleTimeoutSec")), QVariant(540));
    unregisterPowerDevil(bus);
}

void PowerDevilIdleAdapterTest::reportsAbsentOwnerAndRecoversAfterRestart()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter adapter(bus);
    adapter.start();
    QTRY_VERIFY(!adapter.available());

    QSignalSpy finished(&adapter,
                       &QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter::applyFinished);
    QVERIFY(!adapter.apply(true, 5));
    QCOMPARE(adapter.error(), QStringLiteral("powerdevil-unavailable"));
    QCOMPARE(finished.count(), 1);
    QVERIFY(!QFile::exists(m_configFile));

    FakePowerDevil fake;
    QVERIFY(registerPowerDevil(bus, fake));
    QTRY_VERIFY(adapter.available());
    QTRY_COMPARE(fake.refreshCount, 1);
    QVERIFY(adapter.error().isEmpty());

    QVERIFY(adapter.apply(true, 5));
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(finished.at(1).at(0).toBool(), true);
    unregisterPowerDevil(bus);
    QTRY_VERIFY(!adapter.available());
    QVERIFY(registerPowerDevil(bus, fake));
    QTRY_VERIFY(adapter.available());
    QTRY_COMPARE(fake.refreshCount, 3);
    unregisterPowerDevil(bus);
}

void PowerDevilIdleAdapterTest::rejectsInvalidTimeoutWithoutWriting()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePowerDevil fake;
    QVERIFY(registerPowerDevil(bus, fake));
    QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter adapter(bus);
    adapter.start();
    QTRY_VERIFY(adapter.available());

    QSignalSpy finished(&adapter,
                       &QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter::applyFinished);
    QVERIFY(!adapter.apply(true, 0));
    QVERIFY(!adapter.apply(true,
                           QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter::MaximumMinutes + 1));
    QCOMPARE(finished.count(), 2);
    QCOMPARE(fake.refreshCount, 0);
    QVERIFY(!QFile::exists(m_configFile));
    unregisterPowerDevil(bus);
}

void PowerDevilIdleAdapterTest::reportsRefreshFailureAfterConfigSync()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePowerDevil fake;
    fake.failRefresh = true;
    QVERIFY(registerPowerDevil(bus, fake));
    QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter adapter(bus);
    adapter.start();
    QTRY_VERIFY(adapter.available());

    QSignalSpy finished(&adapter,
                       &QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter::applyFinished);
    QVERIFY(adapter.apply(true, 2));
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toBool(), false);
    QVERIFY(adapter.error().startsWith(QStringLiteral("powerdevil-refresh-failed:")));
    QCOMPARE(readDisplayEntry(m_configFile, QStringLiteral("AC"),
                              QStringLiteral("TurnOffDisplayIdleTimeoutSec")), QVariant(120));
    unregisterPowerDevil(bus);
}

void PowerDevilIdleAdapterTest::appliesWhenOwnerBecomesAvailable()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter adapter(bus);
    adapter.start();
    QTRY_VERIFY(!adapter.available());

    QSignalSpy finished(&adapter,
                       &QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter::applyFinished);
    bool applyResult = false;
    connect(&adapter,
            &QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter::availabilityChanged,
            &adapter, [&] {
                if (adapter.available()) {
                    applyResult = adapter.apply(true, 4);
                }
            });

    FakePowerDevil fake;
    QVERIFY(registerPowerDevil(bus, fake));
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(applyResult);
    QVERIFY(!adapter.applying());
    QCOMPARE(finished.at(0).at(0).toBool(), true);
    QCOMPARE(fake.refreshCount, 1);
    unregisterPowerDevil(bus);
}

void PowerDevilIdleAdapterTest::refreshesAfterOwnerReplacement()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePowerDevil fake;
    QVERIFY(registerPowerDevil(bus, fake));
    QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter adapter(bus);
    adapter.start();
    QTRY_VERIFY(adapter.available());

    QVERIFY(QMetaObject::invokeMethod(
        &adapter, "ownerChanged", Qt::DirectConnection,
        Q_ARG(QString, QString::fromLatin1(PowerDevilService)),
        Q_ARG(QString, QStringLiteral(":old-owner")),
        Q_ARG(QString, QStringLiteral(":new-owner"))));
    QTRY_COMPARE(fake.refreshCount, 1);
    QVERIFY(!adapter.applying());
    unregisterPowerDevil(bus);
}

QTEST_MAIN(PowerDevilIdleAdapterTest)
#include "tst_powerdevil_idle_adapter.moc"
