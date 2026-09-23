// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/streaming_preferences/settings1_streaming_preferences.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Services;
using StreamingPreferences::Settings1StreamingPreferences;
using SettingsClientType = QindaQt::Services::SettingsClient::SettingsClient;
using SettingsClient::SettingsTransport;
using SettingsProtocol::SettingsWireStatus;
using SettingsProtocol::WireContract;

namespace {
constexpr auto Port = "services.obsWebSocketPort";
constexpr auto Auto = "services.obsAutoConnect";
constexpr auto Login = "services.obsStartAtLogin";

class FakeTransport final : public SettingsTransport {
    Q_OBJECT
public:
    struct Request { quint64 token; QString owner; };
    QList<Request> snapshots;
    QList<Request> commits;
    bool start(QString *error) override { if (error) error->clear(); return true; }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner, const QStringList &) override {
        snapshots.append({token, owner});
    }
    void commit(quint64 token, const QString &owner, const QString &, quint64,
                const QVariantList &) override { commits.append({token, owner}); }
    void requestActivation() override {}
};

QVariantMap values(int port = 4455, bool autoConnect = true, bool login = false) {
    return {{QLatin1String(Port), port}, {QLatin1String(Auto), autoConnect},
            {QLatin1String(Login), login}};
}
QVariantMap sources() {
    return {{QLatin1String(Port), QStringLiteral("user-overrides")},
            {QLatin1String(Auto), QStringLiteral("user-overrides")},
            {QLatin1String(Login), QStringLiteral("user-overrides")}};
}
QVariantMap snapshotWire(quint64 revision = 1, const QVariantMap &state = values()) {
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch")},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), state},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources()},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}
QVariantMap commitWire(SettingsWireStatus status, const QString &key,
                       const QVariant &value) {
    const bool applied = status == SettingsWireStatus::Applied;
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(status)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch")},
            {QLatin1StringView(WireContract::FieldRevisionBefore), quint64(1)},
            {QLatin1StringView(WireContract::FieldRevisionAfter), quint64(applied ? 2 : 1)},
            {QLatin1StringView(WireContract::FieldValues), QVariantMap{{key, value}}},
            {QLatin1StringView(WireContract::FieldSourceLayers),
             QVariantMap{{key, QStringLiteral("user-overrides")}}},
            {QLatin1StringView(WireContract::FieldChangedKeys),
             applied ? QStringList{key} : QStringList{}},
            {QLatin1StringView(WireContract::FieldMessage),
             applied ? QString{} : QStringLiteral("Rejected by Settings1")}};
}
void baseline(FakeTransport &transport, SettingsClientType &client,
              const QVariantMap &state = values()) {
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.40"));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner, snapshotWire(1, state));
    QTRY_COMPARE(client.state(), SettingsClient::ClientState::Ready);
}
} // namespace

class StreamingPreferencesTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void rejectedWritesNeverPublishRequestedValues();
    void appliedWriteWaitsForAuthoritativeReadback();
    void ownerLossIsUncertainAndDoesNotReplay();
    void failedReadbackReleasesPendingWithoutReplay();
};

void StreamingPreferencesTest::rejectedWritesNeverPublishRequestedValues() {
    FakeTransport transport;
    SettingsClientType client(transport, Settings1StreamingPreferences::scopedKeys(),
                          {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    Settings1StreamingPreferences preferences(client);
    baseline(transport, client);
    QSignalSpy changed(&preferences, &Settings1StreamingPreferences::preferencesChanged);
    const auto reject = [&](const QString &key, const QVariant &current) {
        QTRY_COMPARE(transport.commits.size(), 1);
        const auto request = transport.commits.takeFirst();
        Q_EMIT transport.commitReceived(request.token, request.owner,
                                        commitWire(SettingsWireStatus::ValidationFailed, key, current));
        QTRY_COMPARE(transport.snapshots.size(), 1);
        const auto refresh = transport.snapshots.takeFirst();
        Q_EMIT transport.snapshotReceived(refresh.token, refresh.owner, snapshotWire());
        QTRY_VERIFY(preferences.isLoaded());
        QVERIFY2(preferences.writeStatusText().contains(QStringLiteral("Rejected")),
                 qPrintable(preferences.writeStatusText()));
    };
    QVERIFY(preferences.setWebSocketPort(4466));
    QCOMPARE(preferences.webSocketPort(), 4455);
    QCOMPARE(changed.size(), 0);
    reject(QLatin1String(Port), 4455);
    QVERIFY(preferences.setAutoConnect(false));
    QVERIFY(preferences.autoConnect());
    reject(QLatin1String(Auto), true);
    QVERIFY(preferences.setStartObsAtLogin(true));
    QVERIFY(!preferences.startObsAtLogin());
    reject(QLatin1String(Login), false);
    QCOMPARE(preferences.webSocketPort(), 4455);
    QVERIFY(preferences.autoConnect());
    QVERIFY(!preferences.startObsAtLogin());
}

void StreamingPreferencesTest::appliedWriteWaitsForAuthoritativeReadback() {
    FakeTransport transport;
    SettingsClientType client(transport, Settings1StreamingPreferences::scopedKeys(),
                          {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    Settings1StreamingPreferences preferences(client);
    baseline(transport, client);
    QVERIFY(preferences.setWebSocketPort(4466));
    QVERIFY(preferences.writePending());
    QVERIFY(!preferences.setAutoConnect(false)); // serialization, no queued replay
    const auto commit = transport.commits.takeFirst();
    Q_EMIT transport.commitReceived(commit.token, commit.owner,
                                    commitWire(SettingsWireStatus::Applied, QLatin1String(Port), 4466));
    QVERIFY(preferences.writePending());
    QCOMPARE(preferences.webSocketPort(), 4455);
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto refresh = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(refresh.token, refresh.owner,
                                      snapshotWire(2, values(4466)));
    QTRY_VERIFY(preferences.isLoaded());
    QVERIFY(!preferences.writePending());
    QCOMPARE(preferences.webSocketPort(), 4466);
}

void StreamingPreferencesTest::ownerLossIsUncertainAndDoesNotReplay() {
    FakeTransport transport;
    SettingsClientType client(transport, Settings1StreamingPreferences::scopedKeys(),
                          {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    Settings1StreamingPreferences preferences(client);
    baseline(transport, client);
    QVERIFY(preferences.setStartObsAtLogin(true));
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.41"));
    QVERIFY(!preferences.isLoaded());
    QVERIFY(!preferences.writePending());
    QVERIFY(preferences.writeStatusText().contains(QStringLiteral("not confirmed")));
    QCOMPARE(transport.commits.size(), 1);
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto refresh = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(refresh.token, refresh.owner,
                                      snapshotWire(0, values()));
    QTRY_VERIFY(preferences.isLoaded());
    QCOMPARE(transport.commits.size(), 1);
    QVERIFY(!preferences.startObsAtLogin());
}

void StreamingPreferencesTest::failedReadbackReleasesPendingWithoutReplay() {
    FakeTransport transport;
    SettingsClientType client(transport, Settings1StreamingPreferences::scopedKeys(),
                          {.requestTimeoutMilliseconds = 300, .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    Settings1StreamingPreferences preferences(client);
    baseline(transport, client);
    QVERIFY(preferences.setStartObsAtLogin(true));
    const auto commit = transport.commits.takeFirst();
    Q_EMIT transport.commitReceived(commit.token, commit.owner,
                                    commitWire(SettingsWireStatus::Applied, QLatin1String(Login), true));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    // Deliberately withhold the authoritative readback until SettingsClient
    // times out. The previous false remains published and no write is replayed.
    QTRY_VERIFY(!preferences.writePending());
    QVERIFY(!preferences.isLoaded());
    QVERIFY(!preferences.startObsAtLogin());
    QVERIFY(preferences.writeStatusText().contains(QStringLiteral("not confirmed")));
    QCOMPARE(transport.commits.size(), 0);
}

QTEST_GUILESS_MAIN(StreamingPreferencesTest)
#include "tst_streaming_preferences.moc"
