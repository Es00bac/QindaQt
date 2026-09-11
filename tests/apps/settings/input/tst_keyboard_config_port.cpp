// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/keyboard_config_port.h>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QFile>
#include <QIODevice>
#include <QTemporaryDir>
#include <QTest>

#include "support/private_bus.h"

using QindaQt::Apps::SettingsInput::isValidKeyboardConfig;
using QindaQt::Apps::SettingsInput::KeyboardConfig;
using QindaQt::Apps::SettingsInput::QtKeyboardConfigPort;
using QindaQt::Apps::SettingsInput::StoreResult;

// Fake desktop authority answering reconfigure on a private bus. Q_OBJECT
// classes cannot live in function scope.
class FakeKWin : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin")
public:
    int reconfigures = 0;
public Q_SLOTS:
    Q_SCRIPTABLE void reconfigure() { ++reconfigures; }
};

class KeyboardConfigPortTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void missingFileIsDefaultTruth();
    void roundTripPersistsValues();
    void rejectsOutOfRangeValuesWithoutWriting();
    void reloadFailureIsReportedSeparately();
    void reloadSuccessIsReported();
    void validityHelperRejectsOutOfRange();

private:
    QString configPath() const
    {
        // Unique per test: several rows assert on the file's existence or
        // absence, so sharing one name would leak state between rows.
        return m_dir.filePath(
            QStringLiteral("kcminputrc-%1")
                .arg(QTest::currentTestFunction()));
    }
    QTemporaryDir m_dir;
};

void KeyboardConfigPortTest::missingFileIsDefaultTruth()
{
    QtKeyboardConfigPort port(configPath(), QDBusConnection::sessionBus());
    QString error;
    const KeyboardConfig config = port.read(&error);
    QVERIFY(error.isEmpty());
    QVERIFY(config.keyRepeat);
    QCOMPARE(config.repeatDelayMs, 500);
    QCOMPARE(config.repeatRate, 25);
    QCOMPARE(config.numLockAtLogin, 2);
}

void KeyboardConfigPortTest::roundTripPersistsValues()
{
    const QString path = configPath();
    QtKeyboardConfigPort port(path, QDBusConnection::sessionBus());
    KeyboardConfig config;
    config.keyRepeat = true;
    config.repeatDelayMs = 660;
    config.repeatRate = 25;
    config.numLockAtLogin = 1;
    QString error;
    // No desktop authority on the dead bus: the write still persists the
    // file but reports the reload separately.
    QCOMPARE(port.write(config, &error),
             StoreResult::StoredButReloadFailed);
    QCOMPARE(error, QString());

    QtKeyboardConfigPort reader(path, QDBusConnection::sessionBus());
    const KeyboardConfig readBack = reader.read(nullptr);
    QVERIFY(readBack.keyRepeat);
    QCOMPARE(readBack.repeatDelayMs, 660);
    QCOMPARE(readBack.repeatRate, 25);
    QCOMPARE(readBack.numLockAtLogin, 1);

    // Exact authority key names, verified against the pinned KWin.
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString contents = QString::fromUtf8(file.readAll());
    QVERIFY(contents.contains(QStringLiteral("[Keyboard]")));
    QVERIFY(contents.contains(QStringLiteral("KeyRepeat=true")));
    QVERIFY(contents.contains(QStringLiteral("RepeatDelay=660")));
    QVERIFY(contents.contains(QStringLiteral("RepeatRate=25")));
    QVERIFY(contents.contains(QStringLiteral("NumLock=1")));
}

void KeyboardConfigPortTest::rejectsOutOfRangeValuesWithoutWriting()
{
    const QString path = configPath();
    QtKeyboardConfigPort port(path, QDBusConnection::sessionBus());
    KeyboardConfig config;
    config.repeatDelayMs = 10; // below 100
    QString error;
    QCOMPARE(port.write(config, &error), StoreResult::Failed);
    QVERIFY(!error.isEmpty());
    config.repeatDelayMs = 5000; // above 2000
    QCOMPARE(port.write(config, &error), StoreResult::Failed);
    config.repeatRate = 0; // below 1
    config.repeatDelayMs = 500;
    QCOMPARE(port.write(config, &error), StoreResult::Failed);
    config.repeatRate = 500;
    QCOMPARE(port.write(config, &error), StoreResult::Failed);
    config.repeatRate = 25;
    config.numLockAtLogin = 7;
    QCOMPARE(port.write(config, &error), StoreResult::Failed);
    // Nothing was written.
    QVERIFY(!QFile::exists(path));
}

void KeyboardConfigPortTest::reloadFailureIsReportedSeparately()
{
    QtKeyboardConfigPort port(configPath(),
                              QDBusConnection(
                                  QStringLiteral("unconnected")));
    KeyboardConfig config;
    QString error;
    QCOMPARE(port.write(config, &error),
             StoreResult::StoredButReloadFailed);
    QCOMPARE(error, QString());
    QVERIFY(QFile::exists(configPath()));
}

void KeyboardConfigPortTest::reloadSuccessIsReported()
{
    // A fake org.kde.KWin on a private bus answers reconfigure, so the
    // write reports full success rather than a degraded one.
    QindaQt::Tests::PrivateBus bus;
    QVERIFY(bus.start());
    FakeKWin fake;
    QVERIFY(bus.connection.registerService(QStringLiteral("org.kde.KWin")));
    QVERIFY(bus.connection.registerObject(
        QStringLiteral("/KWin"), &fake,
        QDBusConnection::ExportAllContents));

    QtKeyboardConfigPort port(configPath(), bus.connection);
    KeyboardConfig config;
    QString error;
    QCOMPARE(port.write(config, &error), StoreResult::Stored);
    QCOMPARE(fake.reconfigures, 1);
}

void KeyboardConfigPortTest::validityHelperRejectsOutOfRange()
{
    KeyboardConfig config;
    QVERIFY(isValidKeyboardConfig(config));
    config.repeatDelayMs = 99;
    QVERIFY(!isValidKeyboardConfig(config));
    config.repeatDelayMs = 2001;
    QVERIFY(!isValidKeyboardConfig(config));
    config.repeatDelayMs = 500;
    config.repeatRate = 101;
    QVERIFY(!isValidKeyboardConfig(config));
    config.repeatRate = 25;
    config.numLockAtLogin = 3;
    QVERIFY(!isValidKeyboardConfig(config));
    config.numLockAtLogin = 2;
    QVERIFY(isValidKeyboardConfig(config));
}

QTEST_MAIN(KeyboardConfigPortTest)
#include "tst_keyboard_config_port.moc"
