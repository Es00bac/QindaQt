// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/settings_service/resident_settings_service.h"
#include "qindaqt/settings/settings_schema.h"

#include <QDBusConnectionInterface>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Services::SettingsService;
using namespace QindaQt::Settings;

class SettingsServiceLifecycleTests final : public QObject {
    Q_OBJECT
private slots:
    void ownsRollsBackReleasesAndRestartsOnAPrivateBus();
};

void SettingsServiceLifecycleTests::ownsRollsBackReleasesAndRestartsOnAPrivateBus()
{
    QProcess daemon;
    daemon.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                 {QStringLiteral("--session"), QStringLiteral("--nofork"),
                  QStringLiteral("--print-address=1")});
    QVERIFY2(daemon.waitForStarted(), qPrintable(daemon.errorString()));
    QVERIFY2(daemon.waitForReadyRead(), qPrintable(daemon.errorString()));
    const QString address = QString::fromUtf8(daemon.readLine()).trimmed();
    QVERIFY(!address.isEmpty());
    const QString connectionName = QStringLiteral("qindaqt-settings-test-%1")
                                       .arg(QCoreApplication::applicationPid());
    auto bus = QDBusConnection::connectToBus(address, connectionName);
    QVERIFY(bus.isConnected());
    const QString replacementConnectionName = connectionName + QStringLiteral("-replacement");
    auto replacementBus = QDBusConnection::connectToBus(address, replacementConnectionName);
    QVERIFY(replacementBus.isConnected());

    QString error;
    auto active = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"), nullptr, &error);
    auto legacy = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v1.json"), nullptr, &error, 1);
    QVERIFY2(active && legacy, qPrintable(error));
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("user.json"));
    ResidentSettingsService first(bus, *active, *legacy, path);
    QVERIFY2(first.start().ok(), "first service did not start");
    QVERIFY(bus.interface()->isServiceRegistered(QStringLiteral("org.qindaqt.Settings1")));

    ResidentSettingsService collision(replacementBus, *active, *legacy, path);
    QVERIFY(collision.start().status == SettingsServiceStartStatus::NameOwnershipConflict);
    first.stop();
    QVERIFY(!bus.interface()->isServiceRegistered(QStringLiteral("org.qindaqt.Settings1")));
    QVERIFY2(collision.start().ok(), "replacement service did not start");
    QVERIFY(!collision.epoch().isEmpty());
    collision.stop();

    QDBusConnection::disconnectFromBus(connectionName);
    QDBusConnection::disconnectFromBus(replacementConnectionName);
    daemon.terminate();
    QVERIFY(daemon.waitForFinished());
}

QTEST_GUILESS_MAIN(SettingsServiceLifecycleTests)
#include "tst_settings_service_lifecycle.moc"
