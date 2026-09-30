// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/night_light/night_light_config_port.h>
#include <qindaqt/services/night_light/night_light_settings_importer.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/services/settings_protocol/settings_wire_status.h>

#include <QTimer>
#include <QtTest>

using namespace QindaQt::Services::NightLight;
using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::SettingsProtocol;
using namespace QindaQt::Services::NightLight;

namespace {
const QString Marker = QStringLiteral("display.nightLight.legacyImported");
const QStringList Keys{
    QStringLiteral("display.nightLight.active"),
    QStringLiteral("display.nightLight.mode"),
    QStringLiteral("display.nightLight.dayTemperatureKelvin"),
    QStringLiteral("display.nightLight.nightTemperatureKelvin"),
    QStringLiteral("display.nightLight.scheduleSource"),
    QStringLiteral("display.nightLight.automaticLocation"),
    QStringLiteral("display.nightLight.latitudeDegrees"),
    QStringLiteral("display.nightLight.longitudeDegrees"),
    QStringLiteral("display.nightLight.sunriseStart"),
    QStringLiteral("display.nightLight.sunsetStart"),
    QStringLiteral("display.nightLight.transitionSeconds"),
    QStringLiteral("display.nightLight.disabledOutputs"),
    Marker};

class FakeTransport final : public SettingsTransport {
    Q_OBJECT
public:
    bool start(QString *) override { return true; }
    void stop() override {}
    void requestActivation() override {}
    void requestSnapshot(quint64 token, const QString &owner, const QStringList &) override
    {
        QTimer::singleShot(0, this, [this, token, owner] {
            QVariantMap wire{
                {QString::fromLatin1(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
                {QString::fromLatin1(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
                {QString::fromLatin1(WireContract::FieldSettingsSchemaVersion), quint32(2)},
                {QString::fromLatin1(WireContract::FieldEpoch), QStringLiteral("night-light-import-epoch")},
                {QString::fromLatin1(WireContract::FieldRevision), revision},
                {QString::fromLatin1(WireContract::FieldValues), values},
                {QString::fromLatin1(WireContract::FieldSourceLayers), layers},
                {QString::fromLatin1(WireContract::FieldMessage), QString{}}};
            Q_EMIT snapshotReceived(token, owner, wire);
        });
    }
    void commit(quint64 token, const QString &owner, const QString &epoch,
                quint64 baseRevision, const QVariantList &operations) override
    {
        Q_UNUSED(epoch);
        const QVariantMap operation = operations.first().toMap();
        const QString key = operation.value(QStringLiteral("key")).toString();
        commitKeys.append(key);
        const bool fail = failNext;
        failNext = false;
        const auto status = fail ? SettingsWireStatus::PersistenceFailed
                                 : SettingsWireStatus::Applied;
        if (!fail) {
            values.insert(key, operation.value(QStringLiteral("value")));
            layers.insert(key, QStringLiteral("user-overrides"));
            ++revision;
            if (!interleaveKey.isEmpty()) {
                values.insert(interleaveKey, interleaveValue);
                layers.insert(interleaveKey, QStringLiteral("user-overrides"));
                ++revision;
                interleaveKey.clear();
            }
        }
        // The commit receipt covers only its own one-step revision. The
        // following native edit is revealed by the subsequent full snapshot.
        const quint64 after = fail ? baseRevision : baseRevision + 1;
        const QVariantMap wire{
            {QString::fromLatin1(WireContract::FieldStatus), quint32(status)},
            {QString::fromLatin1(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QString::fromLatin1(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QString::fromLatin1(WireContract::FieldEpoch), QStringLiteral("night-light-import-epoch")},
            {QString::fromLatin1(WireContract::FieldRevisionBefore), baseRevision},
            {QString::fromLatin1(WireContract::FieldRevisionAfter), after},
            {QString::fromLatin1(WireContract::FieldValues),
             fail ? QVariantMap{{key, values.value(key)}} : QVariantMap{{key, values.value(key)}}},
            {QString::fromLatin1(WireContract::FieldSourceLayers),
             QVariantMap{{key, layers.value(key)}}},
            {QString::fromLatin1(WireContract::FieldChangedKeys),
             fail ? QStringList{} : QStringList{key}},
            {QString::fromLatin1(WireContract::FieldMessage),
             fail ? QStringLiteral("disk full") : QString{}}};
        QTimer::singleShot(0, this, [this, token, owner, wire] {
            Q_EMIT commitReceived(token, owner, wire);
        });
    }

    QVariantMap values{
        {Keys.at(0), true},
        {Keys.at(1), QStringLiteral("DarkLight")},
        {Keys.at(2), qint64(6500)},
        {Keys.at(3), qint64(4500)},
        {Keys.at(4), QStringLiteral("Location")},
        {Keys.at(5), true},
        {Keys.at(6), 0.0},
        {Keys.at(7), 0.0},
        {Keys.at(8), QStringLiteral("06:00:00")},
        {Keys.at(9), QStringLiteral("18:00:00")},
        {Keys.at(10), qint64(1800)},
        {Keys.at(11), QStringList{}},
        {Keys.at(12), false}};
    QVariantMap layers;
    QList<QString> commitKeys;
    quint64 revision = 1;
    bool failNext = false;
    QString interleaveKey;
    QVariant interleaveValue;

    FakeTransport()
    {
        for (const QString &key : Keys)
            layers.insert(key, QStringLiteral("profile-defaults"));
        layers.insert(Keys.at(0), QStringLiteral("user-overrides"));
    }
};

class FakeLegacy final : public NightLightConfigPort {
    Q_OBJECT
public:
    ReadResult result;
    ReadResult read() const override { return result; }
    WriteResult write(const NightLightSettings &) override
    { return {WriteOutcome::Failed, QStringLiteral("legacy writes are forbidden")}; }
};
}

class NightLightSettingsImporterTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void nativeValuesWinAndFailedSaveRetriesBeforePersistingMarker();
    void malformedLegacyInputLeavesMarkerUnset();
    void newlyConfirmedNativeOverrideWinsDuringSequentialImport();
};

void NightLightSettingsImporterTests::nativeValuesWinAndFailedSaveRetriesBeforePersistingMarker()
{
    FakeTransport transport;
    transport.failNext = true;
    SettingsClient client(transport, Keys,
                          {.requestTimeoutMilliseconds = 1000, .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.70"));
    QTRY_VERIFY(client.state() == ClientState::Ready);

    FakeLegacy legacy;
    legacy.result.outcome = NightLightConfigPort::ReadOutcome::Loaded;
    legacy.result.values.output.active = false; // user override is true
    legacy.result.values.schedule.sunriseStart = QTime(5, 30);
    NightLightSettingsImporter importer(client, legacy);
    QSignalSpy changes(&importer, &NightLightSettingsImporter::stateChanged);
    importer.start();
    QTRY_VERIFY(importer.status() == NightLightSettingsImporter::Status::SaveFailed);
    QCOMPARE(transport.commitKeys.first(), Keys.at(8));
    QVERIFY(!transport.commitKeys.contains(Marker));
    QVERIFY(!transport.values.value(Marker).toBool());

    importer.retry();
    QTRY_VERIFY(transport.commitKeys.size() > 1);
    QTRY_VERIFY2(importer.status() == NightLightSettingsImporter::Status::Imported,
        qPrintable(QStringLiteral("status=%1 message=%2 writes=%3")
            .arg(int(importer.status())).arg(importer.message())
            .arg(transport.commitKeys.size())));
    QCOMPARE(transport.values.value(Keys.at(0)).toBool(), true);
    QCOMPARE(transport.values.value(Keys.at(8)).toString(), QStringLiteral("05:30:00"));
    QVERIFY(transport.values.value(Marker).toBool());
    QCOMPARE(transport.commitKeys.last(), Marker);
    QVERIFY(changes.size() >= 4);
    client.stop();
}

void NightLightSettingsImporterTests::malformedLegacyInputLeavesMarkerUnset()
{
    FakeTransport transport;
    SettingsClient client(transport, Keys);
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.71"));
    QTRY_VERIFY(client.state() == ClientState::Ready);
    FakeLegacy legacy;
    legacy.result.outcome = NightLightConfigPort::ReadOutcome::Failed;
    legacy.result.diagnostic = QStringLiteral("invalid stored temperature");
    NightLightSettingsImporter importer(client, legacy);
    importer.start();
    QVERIFY(importer.status() == NightLightSettingsImporter::Status::InvalidLegacy);
    QCOMPARE(importer.message(), QStringLiteral("invalid stored temperature"));
    QVERIFY(transport.commitKeys.isEmpty());
    QVERIFY(!transport.values.value(Marker).toBool());
    client.stop();
}


void NightLightSettingsImporterTests::newlyConfirmedNativeOverrideWinsDuringSequentialImport()
{
    FakeTransport transport;
    const QString importedDay = Keys.at(2);
    const QString nativeNight = Keys.at(3);
    transport.interleaveKey = nativeNight;
    transport.interleaveValue = qint64(5100);
    SettingsClient client(transport, Keys,
                          {.requestTimeoutMilliseconds = 1000, .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.72"));
    QTRY_VERIFY(client.state() == ClientState::Ready);

    FakeLegacy legacy;
    legacy.result.outcome = NightLightConfigPort::ReadOutcome::Loaded;
    legacy.result.values.output.dayTemperatureKelvin = 6200;
    legacy.result.values.output.nightTemperatureKelvin = 4100;
    NightLightSettingsImporter importer(client, legacy);
    importer.start();
    QTRY_VERIFY2(importer.status() == NightLightSettingsImporter::Status::Imported,
        qPrintable(QStringLiteral("status=%1 message=%2")
            .arg(int(importer.status())).arg(importer.message())));

    QVERIFY(transport.commitKeys.contains(importedDay));
    QVERIFY(!transport.commitKeys.contains(nativeNight));
    QCOMPARE(transport.values.value(nativeNight).toLongLong(), qint64(5100));
    QCOMPARE(transport.layers.value(nativeNight).toString(), QStringLiteral("user-overrides"));
    QCOMPARE(transport.commitKeys.last(), Marker);
    client.stop();
}

QTEST_GUILESS_MAIN(NightLightSettingsImporterTests)
#include "tst_night_light_settings_importer.moc"
