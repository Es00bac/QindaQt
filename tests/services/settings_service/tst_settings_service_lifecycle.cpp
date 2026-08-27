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
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

class SettingsChangedCounter final : public QObject {
    Q_OBJECT
public:
    int count = 0;

public Q_SLOTS:
    void receive(const QString &, qulonglong, const QStringList &) { ++count; }
};

namespace {

QDBusPendingCall requestSnapshot(const QDBusConnection &connection,
                                 const QStringList &keys)
{
    auto message = QDBusMessage::createMethodCall(
        QString::fromLatin1(WireContract::ServiceName),
        QString::fromLatin1(WireContract::ObjectPath),
        QString::fromLatin1(WireContract::InterfaceName),
        QString::fromLatin1(WireContract::GetSnapshotMethod));
    message << keys;
    return connection.asyncCall(message, 5'000);
}

QDBusPendingCall requestCommit(const QDBusConnection &connection,
                               const QString &epoch,
                               quint64 baseRevision,
                               const QVariantList &operations)
{
    auto message = QDBusMessage::createMethodCall(
        QString::fromLatin1(WireContract::ServiceName),
        QString::fromLatin1(WireContract::ObjectPath),
        QString::fromLatin1(WireContract::InterfaceName),
        QString::fromLatin1(WireContract::CommitUserTransactionMethod));
    message << epoch << baseRevision << operations;
    return connection.asyncCall(message, 5'000);
}

void verifyUnknownKeyTransactions(const QDBusConnection &serviceBus,
                                  QDBusConnection clientBus,
                                  const ResidentSettingsService &service,
                                  const QString &storagePath)
{
    SettingsChangedCounter changedCounter;
    QVERIFY(clientBus.connect(
        serviceBus.baseService(), QString::fromLatin1(WireContract::ObjectPath),
        QString::fromLatin1(WireContract::InterfaceName),
        QString::fromLatin1(WireContract::SettingsChangedSignal),
        &changedCounter, SLOT(receive(QString,qulonglong,QStringList))));
    for (const QString &kind : {QString::fromLatin1(WireContract::OperationKindSet),
                                QString::fromLatin1(WireContract::OperationKindRemove)}) {
        QVariantMap operation{{QLatin1StringView(WireContract::FieldKey),
                               QStringLiteral("unknown.key")},
                              {QLatin1StringView(WireContract::FieldKind), kind}};
        if (kind == QLatin1StringView(WireContract::OperationKindSet)) {
            operation.insert(QLatin1StringView(WireContract::FieldValue), true);
        }
        QDBusPendingCallWatcher watcher(requestCommit(
            clientBus, service.epoch(), 0, {operation}));
        QTRY_VERIFY_WITH_TIMEOUT(watcher.isFinished(), 5'000);
        const QDBusPendingReply<QVariantMap> reply(watcher);
        QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));
        const QVariantMap wire = reply.value();
        QCOMPARE(wire.size(), WireContract::CommitReplyFieldCount);
        QCOMPARE(wire.value(QLatin1StringView(WireContract::FieldStatus)).toUInt(),
                 quint32(SettingsWireStatus::UnknownKey));
        QCOMPARE(wire.value(QLatin1StringView(WireContract::FieldEpoch)).toString(),
                 service.epoch());
        const QVariant before = wire.value(
            QLatin1StringView(WireContract::FieldRevisionBefore));
        const QVariant after = wire.value(
            QLatin1StringView(WireContract::FieldRevisionAfter));
        QCOMPARE(before.metaType().id(), QMetaType::ULongLong);
        QCOMPARE(after.metaType().id(), QMetaType::ULongLong);
        QCOMPARE(before.toULongLong(), quint64(0));
        QCOMPARE(after.toULongLong(), quint64(0));
        const auto values = QindaQt::Services::SettingsProtocol::decodeBoundedVariantMap(
            wire.value(QLatin1StringView(WireContract::FieldValues)), 1);
        const auto sources = QindaQt::Services::SettingsProtocol::decodeBoundedVariantMap(
            wire.value(QLatin1StringView(WireContract::FieldSourceLayers)), 1);
        const auto changed = QindaQt::Services::SettingsProtocol::decodeBoundedKeyList(
            wire.value(QLatin1StringView(WireContract::FieldChangedKeys)), 1);
        QVERIFY(values && values->isEmpty());
        QVERIFY(sources && sources->isEmpty());
        QVERIFY(changed && changed->isEmpty());
        const QVariant message = wire.value(QLatin1StringView(WireContract::FieldMessage));
        QCOMPARE(message.metaType().id(), QMetaType::QString);
        QVERIFY(message.toString().contains(QStringLiteral("unknown.key")));
        QVERIFY(!message.toString().contains(QChar::Null));
        QVERIFY(message.toString().toUtf8().size() <= WireContract::MaximumMessageBytes);
        QCOMPARE(service.revision(), quint64(0));
        QVERIFY(!QFileInfo::exists(storagePath));
    }
    QTest::qWait(25);
    QCOMPARE(changedCounter.count, 0);

    QDBusPendingCallWatcher snapshotWatcher(requestSnapshot(
        clientBus, {QStringLiteral("services.doNotDisturb")}));
    QTRY_VERIFY_WITH_TIMEOUT(snapshotWatcher.isFinished(), 5'000);
    const QDBusPendingReply<QVariantMap> snapshotReply(snapshotWatcher);
    QVERIFY2(snapshotReply.isValid(), qPrintable(snapshotReply.error().message()));
    QCOMPARE(snapshotReply.value()
                 .value(QLatin1StringView(WireContract::FieldRevision)).toULongLong(),
             quint64(0));
    const auto unchangedValues =
        QindaQt::Services::SettingsProtocol::decodeBoundedVariantMap(
            snapshotReply.value().value(QLatin1StringView(WireContract::FieldValues)), 1);
    QVERIFY(unchangedValues);
    const auto unchangedDnd = QindaQt::Services::SettingsProtocol::decodeBoundedJsonValue(
        unchangedValues->value(QStringLiteral("services.doNotDisturb")));
    QVERIFY(unchangedDnd);
    QCOMPARE(unchangedDnd->metaType().id(), QMetaType::Bool);
    QCOMPARE(unchangedDnd->toBool(), false);
}

} // namespace

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

    QDBusPendingCallWatcher profileWatcher(requestSnapshot(
        replacementBus, {QStringLiteral("appearance.animationDurationMs")}));
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

    verifyUnknownKeyTransactions(bus, replacementBus, first, path);

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
    QDBusPendingCallWatcher nodeWatcher(requestCommit(
        replacementBus, first.epoch(), 0, {nodeOperation}));
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
    QDBusPendingCallWatcher aggregateWatcher(requestCommit(
        replacementBus, first.epoch(), 0, transactionOperations));
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
