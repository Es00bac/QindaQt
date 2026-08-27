// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/settings_service/resident_settings_service.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_decode.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"
#include "qindaqt/settings/settings_document.h"
#include "qindaqt/settings/settings_schema.h"

#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Services::SettingsService;
using namespace QindaQt::Settings;

class SettingsServiceLifecycleTests final : public QObject {
    Q_OBJECT
private slots:
    void ownsRollsBackReleasesAndRestartsOnAPrivateBus();
    void validatesProfileAndUserCompatibilityDocuments();
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
    const QString profileDefaults = QStringLiteral(
        QINDAQT_SOURCE_DIR "/data/settings/profile-defaults/qindaqt.json");
    ResidentSettingsService first(bus, *active, *legacy, profileDefaults, path);
    QVERIFY2(first.start().ok(), "first service did not start");
    QVERIFY(bus.interface()->isServiceRegistered(QStringLiteral("org.qindaqt.Settings1")));

    auto snapshotCall = [](const QDBusConnection &connection) {
        auto message = QDBusMessage::createMethodCall(
            QStringLiteral("org.qindaqt.Settings1"),
            QStringLiteral("/org/qindaqt/Settings1"),
            QStringLiteral("org.qindaqt.Settings1"), QStringLiteral("GetSnapshot"));
        message << QStringList{QStringLiteral("appearance.animationDurationMs")};
        return connection.asyncCall(message, 5'000);
    };
    QDBusPendingCallWatcher profileWatcher(snapshotCall(replacementBus));
    QTRY_VERIFY_WITH_TIMEOUT(profileWatcher.isFinished(), 5'000);
    const QDBusPendingReply<QVariantMap> profileSnapshot(profileWatcher);
    QVERIFY2(profileSnapshot.isValid(), qPrintable(profileSnapshot.error().message()));
    const auto profileValues =
        QindaQt::Services::SettingsProtocol::decodeBoundedVariantMap(
            profileSnapshot.value().value(QStringLiteral("values")), 1);
    const auto profileSources =
        QindaQt::Services::SettingsProtocol::decodeBoundedVariantMap(
            profileSnapshot.value().value(QStringLiteral("sourceLayers")), 1);
    QVERIFY(profileValues && profileSources);
    const auto animation = QindaQt::Services::SettingsProtocol::decodeBoundedJsonValue(
        profileValues->value(QStringLiteral("appearance.animationDurationMs")));
    QVERIFY(animation);
    QCOMPARE(animation->toInt(), 160);
    QCOMPARE(profileSources->value(QStringLiteral("appearance.animationDurationMs")).toString(),
             QStringLiteral("profile-defaults"));

    auto commitCall = [&replacementBus, &first](const QVariantList &operations) {
        auto message = QDBusMessage::createMethodCall(
            QStringLiteral("org.qindaqt.Settings1"),
            QStringLiteral("/org/qindaqt/Settings1"),
            QStringLiteral("org.qindaqt.Settings1"),
            QStringLiteral("CommitUserTransaction"));
        message << first.epoch() << quint64(0) << operations;
        return replacementBus.asyncCall(message, 5'000);
    };
    QVariantList nodeOverflow;
    for (qsizetype index = 0;
         index < QindaQt::Services::SettingsProtocol::WireContract::MaximumListEntries - 12;
         ++index) {
        nodeOverflow.append(QVariant::fromValue(QVariantList(8, true)));
    }
    const QVariantMap nodeOperation{
        {QStringLiteral("key"), QStringLiteral("displays.configuration")},
        {QStringLiteral("kind"), QStringLiteral("set")},
        {QStringLiteral("value"), nodeOverflow}};
    QDBusPendingCallWatcher nodeWatcher(commitCall({nodeOperation}));
    QTRY_VERIFY_WITH_TIMEOUT(nodeWatcher.isFinished(), 5'000);
    const QDBusPendingReply<QVariantMap> nodeReply(nodeWatcher);
    QVERIFY(nodeReply.isValid());
    QCOMPARE(nodeReply.value().value(QStringLiteral("status")).toUInt(),
             quint32(QindaQt::Services::SettingsProtocol::SettingsWireStatus::MalformedRequest));
    QCOMPARE(first.revision(), quint64(0));
    QVERIFY(!QFileInfo::exists(path));

    QVariantList transactionOperations;
    const QVariantList largeValue(15, QString(16'000, QLatin1Char('x')));
    for (int index = 0; index < 5; ++index) {
        transactionOperations.append(QVariant::fromValue(QVariantMap{
            {QStringLiteral("key"), QStringLiteral("test.aggregate.%1").arg(index)},
            {QStringLiteral("kind"), QStringLiteral("set")},
            {QStringLiteral("value"), largeValue}}));
    }
    QDBusPendingCallWatcher aggregateWatcher(commitCall(transactionOperations));
    QTRY_VERIFY_WITH_TIMEOUT(aggregateWatcher.isFinished(), 5'000);
    const QDBusPendingReply<QVariantMap> aggregateReply(aggregateWatcher);
    QVERIFY(aggregateReply.isValid());
    QCOMPARE(aggregateReply.value().value(QStringLiteral("status")).toUInt(),
             quint32(QindaQt::Services::SettingsProtocol::SettingsWireStatus::MalformedRequest));
    QCOMPARE(first.revision(), quint64(0));
    QVERIFY(!QFileInfo::exists(path));

    ResidentSettingsService collision(replacementBus, *active, *legacy,
                                      profileDefaults, path);
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

void SettingsServiceLifecycleTests::validatesProfileAndUserCompatibilityDocuments()
{
    QProcess daemon;
    daemon.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                 {QStringLiteral("--session"), QStringLiteral("--nofork"),
                  QStringLiteral("--print-address=1")});
    QVERIFY2(daemon.waitForStarted(), qPrintable(daemon.errorString()));
    QVERIFY2(daemon.waitForReadyRead(), qPrintable(daemon.errorString()));
    const QString address = QString::fromUtf8(daemon.readLine()).trimmed();
    const QString suffix = QString::number(QCoreApplication::applicationPid());
    const QString serviceConnection = QStringLiteral("settings-profile-") + suffix;
    const QString clientConnection = QStringLiteral("settings-profile-client-") + suffix;
    auto bus = QDBusConnection::connectToBus(address, serviceConnection);
    auto clientBus = QDBusConnection::connectToBus(address, clientConnection);
    QVERIFY(bus.isConnected());
    QVERIFY(clientBus.isConnected());

    QString error;
    auto active = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"), nullptr, &error);
    auto legacy = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v1.json"), nullptr, &error, 1);
    QVERIFY2(active && legacy, qPrintable(error));
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("user.json"));
    const QString profileDefaults = QStringLiteral(
        QINDAQT_SOURCE_DIR "/data/settings/profile-defaults/qindaqt.json");

    ResidentSettingsService relativeProfile(bus, *active, *legacy,
                                            QStringLiteral("relative.json"), path);
    QCOMPARE(relativeProfile.start().status,
             SettingsServiceStartStatus::InvalidProfileDefaultsPath);
    ResidentSettingsService missingProfile(
        bus, *active, *legacy, directory.filePath(QStringLiteral("missing.json")), path);
    QCOMPARE(missingProfile.start().status,
             SettingsServiceStartStatus::CorruptProfileDefaults);

    const QString wrongProfilePath = directory.filePath(QStringLiteral("wrong-profile.json"));
    const SettingsDocument wrongProfile{.schemaVersion = 2,
                                        .layer = SettingLayer::UserOverrides,
                                        .values = {}};
    QVERIFY(SettingsFileStore::save(wrongProfilePath, wrongProfile, *active));
    ResidentSettingsService wrongProfileService(bus, *active, *legacy,
                                                wrongProfilePath, path);
    QCOMPARE(wrongProfileService.start().status,
             SettingsServiceStartStatus::CorruptProfileDefaults);

    const QString futureProfilePath = directory.filePath(QStringLiteral("future-profile.json"));
    QFile futureProfile(futureProfilePath);
    QVERIFY(futureProfile.open(QIODevice::WriteOnly));
    QVERIFY(futureProfile.write(
        R"json({"schemaVersion":99,"layer":"profile-defaults","values":{}})json") > 0);
    futureProfile.close();
    ResidentSettingsService futureProfileService(bus, *active, *legacy,
                                                 futureProfilePath, path);
    QCOMPARE(futureProfileService.start().status,
             SettingsServiceStartStatus::CorruptProfileDefaults);

    const QString legacyProfilePath = directory.filePath(QStringLiteral("legacy-profile.json"));
    const SettingsDocument legacyProfile{
        .schemaVersion = 1,
        .layer = SettingLayer::ProfileDefaults,
        .values = {{QStringLiteral("appearance.animationDurationMs"), 155}}};
    QVERIFY(SettingsFileStore::save(legacyProfilePath, legacyProfile, *legacy));
    const QString legacyStorage = directory.filePath(QStringLiteral("legacy-profile-user.json"));
    ResidentSettingsService legacyProfileService(bus, *active, *legacy,
                                                 legacyProfilePath, legacyStorage);
    QVERIFY2(legacyProfileService.start().ok(), "legacy profile service did not start");
    auto message = QDBusMessage::createMethodCall(
        QStringLiteral("org.qindaqt.Settings1"), QStringLiteral("/org/qindaqt/Settings1"),
        QStringLiteral("org.qindaqt.Settings1"), QStringLiteral("GetSnapshot"));
    message << QStringList{QStringLiteral("appearance.animationDurationMs")};
    QDBusPendingCallWatcher legacyWatcher(clientBus.asyncCall(message, 5'000));
    QTRY_VERIFY_WITH_TIMEOUT(legacyWatcher.isFinished(), 5'000);
    const QDBusPendingReply<QVariantMap> legacySnapshot(legacyWatcher);
    QVERIFY(legacySnapshot.isValid());
    const auto legacyValues = QindaQt::Services::SettingsProtocol::decodeBoundedVariantMap(
        legacySnapshot.value().value(QStringLiteral("values")), 1);
    QVERIFY(legacyValues);
    const auto animation = QindaQt::Services::SettingsProtocol::decodeBoundedJsonValue(
        legacyValues->value(QStringLiteral("appearance.animationDurationMs")));
    QVERIFY(animation);
    QCOMPARE(animation->toInt(), 155);
    QVERIFY(!SettingsFileStore::load(legacyProfilePath, *active).ok);
    QVERIFY(SettingsFileStore::load(legacyProfilePath, *legacy).ok);
    legacyProfileService.stop();

    const QString wrongUserPath = directory.filePath(QStringLiteral("wrong-user.json"));
    const SettingsDocument wrongUser{.schemaVersion = 2,
                                     .layer = SettingLayer::ProfileDefaults,
                                     .values = {}};
    QVERIFY(SettingsFileStore::save(wrongUserPath, wrongUser, *active));
    ResidentSettingsService wrongUserService(bus, *active, *legacy,
                                             profileDefaults, wrongUserPath);
    QCOMPARE(wrongUserService.start().status,
             SettingsServiceStartStatus::CorruptUserOverrides);

    const QString legacyUserPath = directory.filePath(QStringLiteral("legacy-user.json"));
    const SettingsDocument legacyUser{
        .schemaVersion = 1,
        .layer = SettingLayer::UserOverrides,
        .values = {{QStringLiteral("appearance.theme"), QStringLiteral("qinda-light")}}};
    QVERIFY(SettingsFileStore::save(legacyUserPath, legacyUser, *legacy));
    ResidentSettingsService legacyUserService(bus, *active, *legacy,
                                              profileDefaults, legacyUserPath);
    QVERIFY2(legacyUserService.start().ok(), "legacy user service did not start");
    const auto persistedMigration = SettingsFileStore::load(legacyUserPath, *active);
    QVERIFY2(persistedMigration.ok, qPrintable(persistedMigration.error));
    QCOMPARE(persistedMigration.sourceSchemaVersion, 2);
    QVERIFY(persistedMigration.document.layer == SettingLayer::UserOverrides);
    legacyUserService.stop();

    const QString oversizedUserPath = directory.filePath(QStringLiteral("oversized-user.json"));
    const SettingsDocument oversizedUser{
        .schemaVersion = 2,
        .layer = SettingLayer::UserOverrides,
        .values = {{QStringLiteral("displays.configuration"),
                    QVariantMap{{QStringLiteral("payload"),
                                 QString(QindaQt::Services::SettingsProtocol::WireContract::
                                             MaximumStringValueBytes + 1,
                                         QLatin1Char('x'))}}}}};
    QVERIFY(SettingsFileStore::save(oversizedUserPath, oversizedUser, *active));
    ResidentSettingsService oversizedUserService(bus, *active, *legacy,
                                                 profileDefaults, oversizedUserPath);
    const auto oversizedStart = oversizedUserService.start();
    QCOMPARE(oversizedStart.status, SettingsServiceStartStatus::CorruptUserOverrides);
    QVERIFY(oversizedStart.message.contains(QStringLiteral("UTF-8 bytes")));

    QDBusConnection::disconnectFromBus(serviceConnection);
    QDBusConnection::disconnectFromBus(clientConnection);
    daemon.terminate();
    QVERIFY(daemon.waitForFinished());
}

QTEST_GUILESS_MAIN(SettingsServiceLifecycleTests)
#include "tst_settings_service_lifecycle.moc"
