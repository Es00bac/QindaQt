// SPDX-License-Identifier: LGPL-3.0-or-later
// The screenshot preferences against a fake Settings1 transport, and the
// schema they must mirror (data/settings/schema-v2.json).
#include <qindaqt/services/screenshot_preferences/settings1_screenshot_preferences.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Services;
using ScreenshotPreferences::Settings1ScreenshotPreferences;
using SettingsClientType = QindaQt::Services::SettingsClient::SettingsClient;
using SettingsClient::SettingsTransport;
using SettingsProtocol::SettingsWireStatus;
using SettingsProtocol::WireContract;

namespace {

class FakeTransport final : public SettingsTransport {
    Q_OBJECT
public:
    struct Request { quint64 token; QString owner; };
    QList<Request> snapshots;
    QList<Request> commits;
    bool start(QString *error) override { if (error) error->clear(); return true; }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner, const QStringList &) override
    {
        snapshots.append({token, owner});
    }
    void commit(quint64 token, const QString &owner, const QString &, quint64, const QVariantList &) override
    {
        commits.append({token, owner});
    }
    void requestActivation() override {}
};

QVariantMap defaults()
{
    return {{QStringLiteral("services.screenshotFolder"), QString()},
            {QStringLiteral("services.screenshotFileNamePattern"), QStringLiteral("Screenshot_{date}_{time}")},
            {QStringLiteral("services.screenshotDefaultMode"), QStringLiteral("region")},
            {QStringLiteral("services.screenshotDelaySeconds"), 0},
            {QStringLiteral("services.screenshotShowResult"), true},
            {QStringLiteral("services.screenshotRecordFinish"), QStringLiteral("notify")}};
}

QVariantMap sourcesFor(const QVariantMap &values)
{
    QVariantMap sources;
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        sources.insert(it.key(), QStringLiteral("user-overrides"));
    return sources;
}

QVariantMap snapshotWire(quint64 revision, const QVariantMap &state)
{
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch")},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), state},
            {QLatin1StringView(WireContract::FieldSourceLayers), sourcesFor(state)},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

QVariantMap commitWire(SettingsWireStatus status, const QString &key, const QVariant &value)
{
    const bool applied = status == SettingsWireStatus::Applied;
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(status)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch")},
            {QLatin1StringView(WireContract::FieldRevisionBefore), quint64(1)},
            {QLatin1StringView(WireContract::FieldRevisionAfter), quint64(applied ? 2 : 1)},
            {QLatin1StringView(WireContract::FieldValues), QVariantMap{{key, value}}},
            {QLatin1StringView(WireContract::FieldSourceLayers), QVariantMap{{key, QStringLiteral("user-overrides")}}},
            {QLatin1StringView(WireContract::FieldChangedKeys), applied ? QStringList{key} : QStringList{}},
            {QLatin1StringView(WireContract::FieldMessage), applied ? QString{} : QStringLiteral("Rejected by Settings1")}};
}

void baseline(FakeTransport &transport, SettingsClientType &client, const QVariantMap &state)
{
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.40"));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner, snapshotWire(1, state));
    QTRY_COMPARE(client.state(), SettingsClient::ClientState::Ready);
}

SettingsClient::ClientTiming fastTiming()
{
    return {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0, .retryMilliseconds = {10}};
}

} // namespace

class ScreenshotPreferencesTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void keysMirrorTheSchema();
    void unavailableSettingsReadAsDefaults();
    void storedValuesOutsideTheirRangeReadAsDefaults();
    void appliedWriteWaitsForReadbackAndNeverReplays();
    void invalidRequestsAreRefusedLocally();
};

void ScreenshotPreferencesTest::keysMirrorTheSchema()
{
    QFile file(QStringLiteral(QINDAQT_SETTINGS_SCHEMA));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonArray settings = QJsonDocument::fromJson(file.readAll()).object()
                                    .value(QStringLiteral("settings")).toArray();
    QHash<QString, QJsonObject> byKey;
    for (const QJsonValue &entry : settings)
        byKey.insert(entry.toObject().value(QStringLiteral("key")).toString(), entry.toObject());
    const QVariantMap expected = defaults();
    for (const QString &key : Settings1ScreenshotPreferences::scopedKeys()) {
        QVERIFY2(byKey.contains(key), qPrintable(key));
        QCOMPARE(byKey.value(key).value(QStringLiteral("domain")).toString(), QStringLiteral("services"));
        QCOMPARE(byKey.value(key).value(QStringLiteral("default")).toVariant(), expected.value(key));
    }
    const auto allowed = [&byKey](const char *key) {
        QStringList values;
        for (const QJsonValue &value : byKey.value(QLatin1String(key)).value(QStringLiteral("constraints"))
                                           .toObject().value(QStringLiteral("allowedValues")).toArray())
            values.append(value.toString());
        return values;
    };
    QCOMPARE(allowed("services.screenshotDefaultMode"), Settings1ScreenshotPreferences::modeIds());
    QCOMPARE(allowed("services.screenshotRecordFinish"), Settings1ScreenshotPreferences::recordFinishIds());
}

void ScreenshotPreferencesTest::unavailableSettingsReadAsDefaults()
{
    FakeTransport transport;
    SettingsClientType client(transport, Settings1ScreenshotPreferences::scopedKeys(), fastTiming());
    Settings1ScreenshotPreferences preferences(client);
    QVERIFY(!preferences.isLoaded());
    QCOMPARE(preferences.folder(), QString());
    QVERIFY(preferences.effectiveFolder().endsWith(QStringLiteral("/Screenshots")));
    QCOMPARE(preferences.fileNamePattern(), QStringLiteral("Screenshot_{date}_{time}"));
    QCOMPARE(preferences.defaultMode(), QStringLiteral("region"));
    QCOMPARE(preferences.delaySeconds(), 0);
    QVERIFY(preferences.showResultWindow());
    QCOMPARE(preferences.recordFinish(), QStringLiteral("notify"));
    QVERIFY(!preferences.setDelaySeconds(3)); // nothing to write to
}

void ScreenshotPreferencesTest::storedValuesOutsideTheirRangeReadAsDefaults()
{
    FakeTransport transport;
    SettingsClientType client(transport, Settings1ScreenshotPreferences::scopedKeys(), fastTiming());
    Settings1ScreenshotPreferences preferences(client);
    QVariantMap state = defaults();
    state.insert(QStringLiteral("services.screenshotFolder"), QStringLiteral("/srv/shots"));
    state.insert(QStringLiteral("services.screenshotDefaultMode"), QStringLiteral("everything"));
    state.insert(QStringLiteral("services.screenshotDelaySeconds"), 99);
    state.insert(QStringLiteral("services.screenshotFileNamePattern"), QStringLiteral("../escape"));
    state.insert(QStringLiteral("services.screenshotShowResult"), false);
    state.insert(QStringLiteral("services.screenshotRecordFinish"), QStringLiteral("show-in-folder"));
    baseline(transport, client, state);
    QTRY_VERIFY(preferences.isLoaded());
    QCOMPARE(preferences.folder(), QStringLiteral("/srv/shots"));
    QCOMPARE(preferences.effectiveFolder(), QStringLiteral("/srv/shots"));
    QCOMPARE(preferences.defaultMode(), QStringLiteral("region"));
    QCOMPARE(preferences.delaySeconds(), 0);
    QCOMPARE(preferences.fileNamePattern(), QStringLiteral("Screenshot_{date}_{time}"));
    QVERIFY(!preferences.showResultWindow());
    QCOMPARE(preferences.recordFinish(), QStringLiteral("show-in-folder"));
}

void ScreenshotPreferencesTest::appliedWriteWaitsForReadbackAndNeverReplays()
{
    FakeTransport transport;
    SettingsClientType client(transport, Settings1ScreenshotPreferences::scopedKeys(), fastTiming());
    Settings1ScreenshotPreferences preferences(client);
    baseline(transport, client, defaults());
    QTRY_VERIFY(preferences.isLoaded());
    QVERIFY(preferences.setDelaySeconds(5));
    QVERIFY(preferences.writePending());
    QVERIFY(!preferences.setShowResultWindow(false)); // one write at a time
    QCOMPARE(transport.commits.size(), 1);
    const auto commit = transport.commits.takeFirst();
    Q_EMIT transport.commitReceived(commit.token, commit.owner,
                                    commitWire(SettingsWireStatus::Applied,
                                               QStringLiteral("services.screenshotDelaySeconds"), 5));
    QVERIFY(preferences.writePending());
    QCOMPARE(preferences.delaySeconds(), 0); // not published before readback
    QTRY_COMPARE(transport.snapshots.size(), 1);
    QVariantMap next = defaults();
    next.insert(QStringLiteral("services.screenshotDelaySeconds"), 5);
    const auto refresh = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(refresh.token, refresh.owner, snapshotWire(2, next));
    QTRY_VERIFY(!preferences.writePending());
    QCOMPARE(preferences.delaySeconds(), 5);
    QCOMPARE(preferences.writeStatusText(), QStringLiteral("Saved."));

    QVERIFY(preferences.setRecordFinish(QStringLiteral("quiet")));
    const auto rejected = transport.commits.takeFirst();
    Q_EMIT transport.commitReceived(rejected.token, rejected.owner,
                                    commitWire(SettingsWireStatus::ValidationFailed,
                                               QStringLiteral("services.screenshotRecordFinish"), QStringLiteral("notify")));
    QTRY_VERIFY(!preferences.writePending());
    QCOMPARE(preferences.recordFinish(), QStringLiteral("notify"));
    QVERIFY(transport.commits.isEmpty()); // never replayed
}

void ScreenshotPreferencesTest::invalidRequestsAreRefusedLocally()
{
    FakeTransport transport;
    SettingsClientType client(transport, Settings1ScreenshotPreferences::scopedKeys(), fastTiming());
    Settings1ScreenshotPreferences preferences(client);
    baseline(transport, client, defaults());
    QTRY_VERIFY(preferences.isLoaded());
    QVERIFY(!preferences.setFileNamePattern(QStringLiteral("a/b")));
    QVERIFY(!preferences.setFolder(QStringLiteral("relative")));
    QVERIFY(!preferences.setDefaultMode(QStringLiteral("everything")));
    QVERIFY(!preferences.setDelaySeconds(61));
    QVERIFY(!preferences.setRecordFinish(QStringLiteral("explode")));
    QVERIFY(transport.commits.isEmpty());
    QVERIFY(preferences.fileNamePreview(QStringLiteral("x/y")).isEmpty());
    QVERIFY(preferences.fileNamePreview(QStringLiteral("Shot_{date}")).startsWith(QStringLiteral("Shot_")));
}

QTEST_GUILESS_MAIN(ScreenshotPreferencesTest)
#include "tst_screenshot_preferences.moc"
