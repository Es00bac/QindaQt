// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/keyboard_config_port.h>

#include <QtDBus/QDBusConnection>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QTemporaryDir>
#include <QTest>

#include "support/config_change_listener.h"
#include "support/private_bus.h"

using QindaQt::Apps::SettingsInput::isValidKeyboardConfig;
using QindaQt::Apps::SettingsInput::KeyboardConfig;
using QindaQt::Apps::SettingsInput::QtKeyboardConfigPort;
using QindaQt::Apps::SettingsInput::StoreResult;

class KeyboardConfigPortTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void missingFileIsDefaultTruth();
    void roundTripPersistsValues();
    void rejectsOutOfRangeValuesWithoutWriting();
    void reloadFailureIsReportedSeparately();
    void announcesTheChangeToARunningDesktop();
    void withoutADesktopTheChangeWaitsForTheNextSession();
    void relocatedFileNamesAreNeverAnnounced();
    void validityHelperRejectsOutOfRange();

private:
    static QDBusConnection offlineBus()
    {
        return QDBusConnection(QStringLiteral("none"));
    }
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
    QtKeyboardConfigPort port(configPath(), offlineBus());
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
    QtKeyboardConfigPort port(path, offlineBus());
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

    QtKeyboardConfigPort reader(path, offlineBus());
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
    QtKeyboardConfigPort port(path, offlineBus());
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

void KeyboardConfigPortTest::announcesTheChangeToARunningDesktop()
{
    // A stand-in for the running desktop owns org.kde.KWin on a private bus;
    // a second connection listens exactly where KWin's config watcher does.
    QindaQt::Tests::PrivateBus bus;
    QVERIFY(bus.start());
    QVERIFY(bus.connection.registerService(QStringLiteral("org.kde.KWin")));
    const QString listenerName = QStringLiteral("kcminputrc-listener");
    QindaQt::Tests::ConfigChangeListener listener;
    QVERIFY(listener.listen(QDBusConnection::connectToBus(bus.address, listenerName),
                            QStringLiteral("kcminputrc")));

    QVERIFY(QDir(m_dir.path()).mkpath(QStringLiteral("announce")));
    QtKeyboardConfigPort port(m_dir.filePath(QStringLiteral("announce/kcminputrc")),
                              bus.connection);
    KeyboardConfig config;
    config.repeatDelayMs = 400;
    QString error;
    QCOMPARE(port.write(config, &error), StoreResult::Stored);
    QTRY_COMPARE(listener.changes.size(), 1);
    const QByteArrayList keys =
        listener.changes.first().value(QStringLiteral("Keyboard"));
    QVERIFY(keys.contains("RepeatDelay"));
    QVERIFY(keys.contains("RepeatRate"));
    QVERIFY(keys.contains("KeyRepeat"));
    QVERIFY(keys.contains("NumLock"));
    QDBusConnection::disconnectFromBus(listenerName);
}

void KeyboardConfigPortTest::withoutADesktopTheChangeWaitsForTheNextSession()
{
    // Negative control: the file is durable, but with nobody owning
    // org.kde.KWin the port must not claim a live change.
    QindaQt::Tests::PrivateBus bus;
    QVERIFY(bus.start());
    QVERIFY(QDir(m_dir.path()).mkpath(QStringLiteral("nodesktop")));
    const QString path = m_dir.filePath(QStringLiteral("nodesktop/kcminputrc"));
    QtKeyboardConfigPort port(path, bus.connection);
    QString error;
    QCOMPARE(port.write(KeyboardConfig{}, &error),
             StoreResult::StoredButReloadFailed);
    QVERIFY(QFile::exists(path));
}

void KeyboardConfigPortTest::relocatedFileNamesAreNeverAnnounced()
{
    QindaQt::Tests::PrivateBus bus;
    QVERIFY(bus.start());
    QVERIFY(bus.connection.registerService(QStringLiteral("org.kde.KWin")));
    const QString listenerName = QStringLiteral("relocated-listener");
    QindaQt::Tests::ConfigChangeListener listener;
    QVERIFY(listener.listen(QDBusConnection::connectToBus(bus.address, listenerName),
                            QStringLiteral("kcminputrc")));
    // configPath() carries a hyphenated name, which is not a D-Bus path element.
    QtKeyboardConfigPort port(configPath(), bus.connection);
    QString error;
    QCOMPARE(port.write(KeyboardConfig{}, &error),
             StoreResult::StoredButReloadFailed);
    QTest::qWait(200);
    QCOMPARE(listener.changes.size(), 0);
    QDBusConnection::disconnectFromBus(listenerName);
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
