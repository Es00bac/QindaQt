// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/session/desktop_controls/idle_display_preferences.h>
#include <qindaqt/session/desktop_controls/powerdevil_idle_preferences_binding.h>
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

using QindaQt::Session::DesktopControls::IdleDisplayPreferences;
using QindaQt::Session::DesktopControls::IdlePreferencesProvider;
using QindaQt::Session::DesktopControls::PowerDevilIdlePreferencesBinding;
using QindaQt::Session::PowerDevilIdle::PowerDevilIdleAdapter;

namespace {

constexpr auto PowerDevilService = "org.kde.Solid.PowerManagement";
constexpr auto PowerDevilPath = "/org/kde/Solid/PowerManagement";

class FakePreferences final : public IdlePreferencesProvider {
public:
    using IdlePreferencesProvider::IdlePreferencesProvider;

    [[nodiscard]] IdleDisplayPreferences currentPreferences() const override
    {
        return m_current;
    }

    void refresh() override { ++refreshCount; }

    void set(IdleDisplayPreferences preferences)
    {
        m_current = preferences;
        Q_EMIT preferencesChanged(m_current);
    }

    int refreshCount = 0;

private:
    IdleDisplayPreferences m_current{true, 10};
};

class FakePowerDevil final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement")

public:
    int attemptCount = 0;
    int refreshCount = 0;
    bool failRefresh = false;

public Q_SLOTS:
    void refreshStatus()
    {
        ++attemptCount;
        if (failRefresh) {
            sendErrorReply(QDBusError::Failed,
                           QStringLiteral("binding-refresh-failed"));
            return;
        }
        ++refreshCount;
    }
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

QVariant readDisplayEntry(const QString &path, const QString &key)
{
    KConfig config(path, KConfig::SimpleConfig);
    config.reparseConfiguration();
    return config.group(QStringLiteral("AC"))
        .group(QStringLiteral("Display"))
        .readEntry(key);
}

} // namespace

class PowerDevilIdlePreferencesBindingTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void coalescesLatestPreferenceWhileApplyIsPending();
    void replaysLatestPreferenceAfterOwnerRestart();
    void reportsFailureWithoutRetryLoop();
    void retriesAfterExplicitSamePreferenceChange();
    void retriesNewerPreferenceAfterInFlightFailure();
    void stopCancelsQueuedDrain();

private:
    QTemporaryDir m_configRoot;
    QString m_configFile;
};

void PowerDevilIdlePreferencesBindingTest::initTestCase()
{
    QVERIFY(m_configRoot.isValid());
    qputenv("XDG_CONFIG_HOME", m_configRoot.path().toUtf8());
    m_configFile = configPath(m_configRoot.path());
}

void PowerDevilIdlePreferencesBindingTest::cleanup()
{
    QFile::remove(m_configFile);
}

void PowerDevilIdlePreferencesBindingTest::coalescesLatestPreferenceWhileApplyIsPending()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePowerDevil fake;
    QVERIFY(registerPowerDevil(bus, fake));
    FakePreferences preferences;
    PowerDevilIdleAdapter adapter(bus);
    PowerDevilIdlePreferencesBinding binding(preferences, adapter);
    QSignalSpy finished(&adapter, &PowerDevilIdleAdapter::applyFinished);
    connect(&adapter, &PowerDevilIdleAdapter::applyingChanged, &adapter, [&] {
        if (adapter.applying()) {
            preferences.set(IdleDisplayPreferences(false, 0));
        }
    });
    binding.start();

    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(fake.refreshCount, 2);
    QCOMPARE(readDisplayEntry(m_configFile, QStringLiteral("TurnOffDisplayWhenIdle")),
             QVariant(false));
    QCOMPARE(readDisplayEntry(m_configFile,
                              QStringLiteral("TurnOffDisplayIdleTimeoutSec")),
             QVariant(600));
    QVERIFY(!adapter.applying());
    unregisterPowerDevil(bus);
}

void PowerDevilIdlePreferencesBindingTest::replaysLatestPreferenceAfterOwnerRestart()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePreferences preferences;
    PowerDevilIdleAdapter adapter(bus);
    PowerDevilIdlePreferencesBinding binding(preferences, adapter);
    binding.start();
    QTRY_VERIFY(!adapter.available());

    preferences.set(IdleDisplayPreferences(true, 6));
    FakePowerDevil fake;
    QVERIFY(registerPowerDevil(bus, fake));
    QTRY_VERIFY(adapter.available());
    QTRY_COMPARE(fake.refreshCount, 2);
    QCOMPARE(readDisplayEntry(m_configFile,
                              QStringLiteral("TurnOffDisplayIdleTimeoutSec")),
             QVariant(360));

    unregisterPowerDevil(bus);
    QTRY_VERIFY(!adapter.available());
    preferences.set(IdleDisplayPreferences(true, 8));
    QVERIFY(registerPowerDevil(bus, fake));
    QTRY_VERIFY(adapter.available());
    QTRY_COMPARE(fake.refreshCount, 4);
    QCOMPARE(readDisplayEntry(m_configFile,
                              QStringLiteral("TurnOffDisplayIdleTimeoutSec")),
             QVariant(480));
    unregisterPowerDevil(bus);
}

void PowerDevilIdlePreferencesBindingTest::reportsFailureWithoutRetryLoop()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePowerDevil fake;
    fake.failRefresh = true;
    QVERIFY(registerPowerDevil(bus, fake));
    FakePreferences preferences;
    PowerDevilIdleAdapter adapter(bus);
    PowerDevilIdlePreferencesBinding binding(preferences, adapter);
    binding.start();

    QTRY_VERIFY(!adapter.applying());
    QCOMPARE(fake.refreshCount, 0);
    QTest::qWait(100);
    QCOMPARE(fake.refreshCount, 0);

    fake.failRefresh = false;
    preferences.set(IdleDisplayPreferences(true, 11));
    QTRY_COMPARE(fake.refreshCount, 1);
    unregisterPowerDevil(bus);
}

void PowerDevilIdlePreferencesBindingTest::retriesAfterExplicitSamePreferenceChange()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePowerDevil fake;
    fake.failRefresh = true;
    QVERIFY(registerPowerDevil(bus, fake));
    FakePreferences preferences;
    PowerDevilIdleAdapter adapter(bus);
    PowerDevilIdlePreferencesBinding binding(preferences, adapter);
    binding.start();

    QTRY_COMPARE(fake.attemptCount, 1);
    preferences.set(preferences.currentPreferences());
    QTRY_COMPARE(fake.attemptCount, 2);
    fake.failRefresh = false;
    preferences.set(preferences.currentPreferences());
    QTRY_COMPARE(fake.refreshCount, 1);
    unregisterPowerDevil(bus);
}

void PowerDevilIdlePreferencesBindingTest::retriesNewerPreferenceAfterInFlightFailure()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePowerDevil fake;
    fake.failRefresh = true;
    QVERIFY(registerPowerDevil(bus, fake));
    FakePreferences preferences;
    PowerDevilIdleAdapter adapter(bus);
    PowerDevilIdlePreferencesBinding binding(preferences, adapter);
    bool changedDuringApply = false;
    connect(&adapter, &PowerDevilIdleAdapter::applyingChanged, &adapter, [&] {
        if (adapter.applying() && !changedDuringApply) {
            changedDuringApply = true;
            preferences.set(IdleDisplayPreferences(true, 11));
        }
    });
    binding.start();

    QTRY_COMPARE(fake.attemptCount, 2);
    QTest::qWait(100);
    QCOMPARE(fake.attemptCount, 2);
    unregisterPowerDevil(bus);
}

void PowerDevilIdlePreferencesBindingTest::stopCancelsQueuedDrain()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePowerDevil fake;
    QVERIFY(registerPowerDevil(bus, fake));
    FakePreferences preferences;
    PowerDevilIdleAdapter adapter(bus);
    PowerDevilIdlePreferencesBinding binding(preferences, adapter);
    binding.start();
    binding.stop();
    QTest::qWait(100);
    QCOMPARE(fake.refreshCount, 0);
    QVERIFY(!adapter.available());
    QVERIFY(!adapter.applying());
    unregisterPowerDevil(bus);
}

QTEST_MAIN(PowerDevilIdlePreferencesBindingTest)
#include "tst_powerdevil_idle_preferences_binding.moc"
