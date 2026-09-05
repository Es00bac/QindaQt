// SPDX-License-Identifier: GPL-3.0-or-later
#include "launcher_persistence.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_service/resident_settings_service.h"
#include "qindaqt/settings/settings_document.h"
#include <QDBusConnection>
#include <QProcess>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Shell::Launcher;

class LauncherSettingsContractTests final : public QObject {
    Q_OBJECT
private slots:
    void shippedSchemaPersistsPinsAcrossServiceRestart();
};

void LauncherSettingsContractTests::shippedSchemaPersistsPinsAcrossServiceRestart()
{
    QProcess daemon;
    const auto cleanup = qScopeGuard([&] {
        QDBusConnection::disconnectFromBus(QStringLiteral("launcher-contract-client"));
        QDBusConnection::disconnectFromBus(QStringLiteral("launcher-contract-service"));
        QDBusConnection::disconnectFromBus(QStringLiteral("launcher-contract-replacement"));
        QDBusConnection::disconnectFromBus(QStringLiteral("launcher-contract-fresh-client"));
        daemon.terminate();
        if (!daemon.waitForFinished(2000)) {
            daemon.kill();
            daemon.waitForFinished(2000);
        }
    });
    daemon.start(QStringLiteral(QINDAQT_DBUS_DAEMON),
                 {QStringLiteral("--session"), QStringLiteral("--nofork"),
                  QStringLiteral("--print-address=1")});
    QVERIFY(daemon.waitForStarted());
    QVERIFY(daemon.waitForReadyRead());
    const QString address = QString::fromUtf8(daemon.readLine()).trimmed();
    const auto serviceBus = QDBusConnection::connectToBus(address,
        QStringLiteral("launcher-contract-service"));
    const auto clientBus = QDBusConnection::connectToBus(address,
        QStringLiteral("launcher-contract-client"));
    QString error;
    const auto active = Settings::SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"), nullptr, &error);
    const auto legacy = Settings::SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v1.json"), nullptr, &error, 1);
    QVERIFY2(active && legacy, qPrintable(error));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto storage = directory.filePath(QStringLiteral("user.json"));
    Services::SettingsService::ResidentSettingsService service(serviceBus, *active, *legacy,
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/profile-defaults/qindaqt.json"), storage);
    const auto started = service.start();
    QVERIFY2(started.ok(), qPrintable(started.message));
    Services::SettingsClient::QtSettingsTransport transport(clientBus);
    // Exercise launcher keys alongside shared runtime settings: an unknown
    // launcher key poisons the whole scoped snapshot, not just the launcher.
    Services::SettingsClient::SettingsClient client(transport,
        {QStringLiteral("accessibility.reducedMotion"), QStringLiteral("panels.autoHideDelayMs"),
         QStringLiteral("services.clipboardHistory"), LauncherPersistenceController::pinnedKey(),
         LauncherPersistenceController::recentKey()});
    LauncherPersistenceController persistence(client);
    QVERIFY2(client.start(&error), qPrintable(error));
    QTRY_VERIFY2_WITH_TIMEOUT(persistence.persistenceReady(), qPrintable(client.lastError() + QStringLiteral(" / ") + persistence.statusText()), 3000);
    QVERIFY2(persistence.statusText().isEmpty(), qPrintable(persistence.statusText()));
    QVERIFY(persistence.pinned().ids().isEmpty());
    const QString application = QStringLiteral("org.qindaqt.TextEditor.desktop");
    QCOMPARE(persistence.pin(application), PersistenceMutation::Applied);
    QTRY_VERIFY_WITH_TIMEOUT(!persistence.writeInFlight() && persistence.persistenceReady(), 3000);
    QCOMPARE(persistence.recordLaunch(application), PersistenceMutation::Applied);
    QTRY_VERIFY_WITH_TIMEOUT(!persistence.writeInFlight() && persistence.persistenceReady(), 3000);
    const auto disk = Settings::SettingsFileStore::load(storage, *active);
    QVERIFY2(disk.ok, qPrintable(disk.error + QStringLiteral("; controller: ") + persistence.statusText()));
    QCOMPARE(disk.document.values.value(LauncherPersistenceController::pinnedKey()).toStringList(),
             QStringList{application});
    QCOMPARE(disk.document.values.value(LauncherPersistenceController::recentKey()).toStringList(),
             QStringList{application});
    const QString originalEpoch = service.epoch();
    service.stop();
    QTRY_VERIFY(!persistence.persistenceReady());
    // AGENT-GUARD: a real service restart has a new unique bus owner and
    // epoch. Reusing this connection with a fresh epoch correctly trips the
    // client's same-owner regression fence; never weaken that fence for a test.
    const auto replacementBus = QDBusConnection::connectToBus(address,
        QStringLiteral("launcher-contract-replacement"));
    QVERIFY(replacementBus.baseService() != serviceBus.baseService());
    Services::SettingsService::ResidentSettingsService replacement(
        replacementBus, *active, *legacy,
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/profile-defaults/qindaqt.json"), storage);
    const auto restarted = replacement.start();
    QVERIFY2(restarted.ok(), qPrintable(restarted.message));
    QVERIFY(replacement.epoch() != originalEpoch);
    QTRY_VERIFY2_WITH_TIMEOUT(persistence.persistenceReady(), qPrintable(client.lastError() + QStringLiteral(" / ") + persistence.statusText()), 3000);
    QVERIFY2(persistence.statusText().isEmpty(), qPrintable(persistence.statusText()));
    QCOMPARE(persistence.pinned().ids(), QStringList{application});
    QCOMPARE(persistence.recent().ids(), QStringList{application});

    // A new shell process starts with an empty controller, rather than merely
    // retaining the old shell's already-populated in-memory projection.
    const auto freshBus = QDBusConnection::connectToBus(address,
        QStringLiteral("launcher-contract-fresh-client"));
    Services::SettingsClient::QtSettingsTransport freshTransport(freshBus);
    Services::SettingsClient::SettingsClient freshClient(freshTransport,
        {LauncherPersistenceController::pinnedKey(), LauncherPersistenceController::recentKey()});
    LauncherPersistenceController freshPersistence(freshClient);
    QVERIFY(freshPersistence.pinned().ids().isEmpty());
    QVERIFY(freshPersistence.recent().ids().isEmpty());
    QVERIFY2(freshClient.start(&error), qPrintable(error));
    QTRY_VERIFY2_WITH_TIMEOUT(freshPersistence.persistenceReady(),
                             qPrintable(freshClient.lastError()), 3000);
    QVERIFY2(freshPersistence.statusText().isEmpty(), qPrintable(freshPersistence.statusText()));
    QCOMPARE(freshPersistence.pinned().ids(), QStringList{application});
    QCOMPARE(freshPersistence.recent().ids(), QStringList{application});
}

QTEST_GUILESS_MAIN(LauncherSettingsContractTests)
#include "tst_launcher_settings_contract.moc"
