// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationquietingsettingsbridge.h"
#include "settingsroutelauncher.h"

#include "qindaqt/services/notification_presentation_policy/notification_interruption_policy.h"
#include "qindaqt/services/notification_presentation_policy/notification_privacy_policy.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_service/resident_settings_service.h"
#include "qindaqt/settings/settings_schema.h"

#include <QProcess>
#include <QtTest>
#include <QTemporaryDir>

using namespace QindaQt::Shell;
using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::NotificationPresentationPolicy;
using namespace QindaQt::Services::SettingsService;
using namespace QindaQt::Settings;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

class BridgeTransport final : public SettingsTransport {
    Q_OBJECT
public:
    bool start(QString *) override { return true; }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner, const QStringList &) override
    { requests.append({token, owner}); }
    void commit(quint64, const QString &, const QString &, quint64, const QVariantList &) override {}
    void requestActivation() override {}
    struct Request { quint64 token; QString owner; };
    QList<Request> requests;
};

namespace {
QVariantMap snapshot(bool enabled, QString epoch, quint64 revision)
{
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), std::move(epoch)},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues),
             QVariantMap{{QStringLiteral("services.doNotDisturb"), enabled}}},
            {QLatin1StringView(WireContract::FieldSourceLayers),
             QVariantMap{{QStringLiteral("services.doNotDisturb"), QStringLiteral("user-overrides")}}},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}
}

class NotificationQuietingSettingsBridgeTests final : public QObject {
    Q_OBJECT
private slots:
    void failsQuietThenRetainsLastConfirmedAcrossLossAndReplacement();
    void ordinaryControllerAndShellReconstructFromPersistedChoice();
    void fixedLauncherExposesOnlyTheNotificationsRoute();
};

void NotificationQuietingSettingsBridgeTests::failsQuietThenRetainsLastConfirmedAcrossLossAndReplacement()
{
    BridgeTransport transport;
    SettingsClient client(transport, {QStringLiteral("services.doNotDisturb")},
                          {.requestTimeoutMilliseconds = 100,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    NotificationInterruptionPolicy policy;
    NotificationPrivacyPolicy privacy;
    NotificationQuietingSettingsBridge bridge(client, policy);
    QVERIFY(policy.doNotDisturbEnabled()); // before any baseline
    QVERIFY(!privacy.privatePresentationAllowed());

    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.40"));
    QTRY_COMPARE(transport.requests.size(), 1);
    auto first = transport.requests.takeFirst();
    Q_EMIT transport.snapshotReceived(first.token, first.owner,
                                      snapshot(false, QStringLiteral("epoch-a"), 0));
    QTRY_VERIFY(!policy.doNotDisturbEnabled());
    QVERIFY(!privacy.privatePresentationAllowed());

    Q_EMIT transport.ownerChanged(QString{});
    QCOMPARE(policy.doNotDisturbEnabled(), false);
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.41"));
    QTRY_COMPARE(transport.requests.size(), 1);
    auto replacement = transport.requests.takeFirst();
    Q_EMIT transport.snapshotReceived(replacement.token, replacement.owner,
                                      snapshot(true, QStringLiteral("epoch-b"), 0));
    QTRY_VERIFY(policy.doNotDisturbEnabled());
    QVERIFY(!privacy.privatePresentationAllowed()); // privacy still outranks DND

    Q_EMIT transport.busDisconnected();
    QCOMPARE(policy.doNotDisturbEnabled(), true);
    QVERIFY(bridge.controller().hasBaseline());
}

void NotificationQuietingSettingsBridgeTests::ordinaryControllerAndShellReconstructFromPersistedChoice()
{
    QProcess daemon;
    daemon.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                 {QStringLiteral("--session"), QStringLiteral("--nofork"),
                  QStringLiteral("--print-address=1")});
    QVERIFY(daemon.waitForStarted());
    QVERIFY(daemon.waitForReadyRead());
    const QString address = QString::fromUtf8(daemon.readLine()).trimmed();
    const QString suffix = QString::number(QCoreApplication::applicationPid());
    const QString serviceConnection = QStringLiteral("quieting-service-") + suffix;
    const QString replacementConnection = QStringLiteral("quieting-replacement-") + suffix;
    const QString clientConnection = QStringLiteral("quieting-client-") + suffix;
    auto serviceBus = QDBusConnection::connectToBus(address, serviceConnection);
    auto replacementBus = QDBusConnection::connectToBus(address, replacementConnection);
    auto clientBus = QDBusConnection::connectToBus(address, clientConnection);
    QVERIFY(serviceBus.isConnected());
    QVERIFY(replacementBus.isConnected());
    QVERIFY(clientBus.isConnected());

    QString error;
    auto active = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"), nullptr, &error);
    auto legacy = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v1.json"), nullptr, &error, 1);
    QVERIFY2(active && legacy, qPrintable(error));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString storage = directory.filePath(QStringLiteral("user.json"));
    const QString profileDefaults = QStringLiteral(
        QINDAQT_SOURCE_DIR "/data/settings/profile-defaults/qindaqt.json");
    ResidentSettingsService first(serviceBus, *active, *legacy, profileDefaults, storage);
    QVERIFY(first.start().ok());

    const ClientTiming timing{.requestTimeoutMilliseconds = 500,
                              .debounceMilliseconds = 0,
                              .retryMilliseconds = {10, 20}};
    // This is the ordinary controller path used by the settings application:
    // save once, destroy every consumer object, then reopen without replay.
    {
        QtSettingsTransport transport(clientBus);
        SettingsClient client(transport, {QStringLiteral("services.doNotDisturb")}, timing);
        DoNotDisturbController controller(client);
        QVERIFY(client.start(&error));
        QTRY_VERIFY_WITH_TIMEOUT(controller.ready() && controller.hasBaseline(), 2'000);
        QVERIFY(controller.requestSet(true));
        QTRY_VERIFY_WITH_TIMEOUT(controller.ready() && controller.enabled(), 2'000);
        QCOMPARE(first.revision(), quint64(1));
    }
    {
        QtSettingsTransport transport(clientBus);
        SettingsClient client(transport, {QStringLiteral("services.doNotDisturb")}, timing);
        DoNotDisturbController controller(client);
        QVERIFY(!controller.hasBaseline());
        QVERIFY(client.start(&error));
        QTRY_VERIFY_WITH_TIMEOUT(controller.ready() && controller.hasBaseline(), 2'000);
        QVERIFY(controller.enabled());
        QCOMPARE(first.revision(), quint64(1)); // reopening did not replay the write
        QVERIFY(controller.requestSet(false));
        QTRY_VERIFY_WITH_TIMEOUT(controller.ready() && !controller.enabled(), 2'000);
        QCOMPARE(first.revision(), quint64(2));
        QCOMPARE(client.snapshot()->sourceLayers
                     .value(QStringLiteral("services.doNotDisturb")).toString(),
                 QStringLiteral("user-overrides"));
    }

    // A reconstructed shell begins fail-quiet. The persisted false value may
    // reach the interruption policy only through this fresh exact-owner
    // baseline, and establishing that baseline must not replay a transaction.
    {
        QtSettingsTransport transport(clientBus);
        SettingsClient client(transport, {QStringLiteral("services.doNotDisturb")}, timing);
        NotificationInterruptionPolicy policy;
        NotificationQuietingSettingsBridge bridge(client, policy);
        QSignalSpy policyChanges(&policy,
                                 &NotificationInterruptionPolicy::doNotDisturbEnabledChanged);
        QVERIFY(policy.doNotDisturbEnabled());
        QVERIFY(!bridge.controller().hasBaseline());
        QCOMPARE(policyChanges.size(), 0);
        QVERIFY(client.start(&error));
        QTRY_VERIFY_WITH_TIMEOUT(bridge.controller().ready()
                                     && bridge.controller().hasBaseline(), 2'000);
        QVERIFY(!policy.doNotDisturbEnabled());
        QCOMPARE(client.snapshot()->sourceLayers
                     .value(QStringLiteral("services.doNotDisturb")).toString(),
                 QStringLiteral("user-overrides"));
        QCOMPARE(policyChanges.size(), 1);
        QCOMPARE(first.revision(), quint64(2));
    }
    {
        QtSettingsTransport transport(clientBus);
        SettingsClient client(transport, {QStringLiteral("services.doNotDisturb")}, timing);
        NotificationInterruptionPolicy policy;
        NotificationQuietingSettingsBridge bridge(client, policy);
        QSignalSpy policyChanges(&policy,
                                 &NotificationInterruptionPolicy::doNotDisturbEnabledChanged);
        QVERIFY(policy.doNotDisturbEnabled());
        QVERIFY(!bridge.controller().hasBaseline());
        QCOMPARE(policyChanges.size(), 0);
        QVERIFY(client.start(&error));
        QTRY_VERIFY_WITH_TIMEOUT(bridge.controller().ready()
                                     && bridge.controller().hasBaseline(), 2'000);
        QVERIFY(!policy.doNotDisturbEnabled());
        QCOMPARE(client.snapshot()->sourceLayers
                     .value(QStringLiteral("services.doNotDisturb")).toString(),
                 QStringLiteral("user-overrides"));
        QCOMPARE(policyChanges.size(), 1);
        QCOMPARE(first.revision(), quint64(2)); // shell reconstruction did not replay
    }

    first.stop();
    ResidentSettingsService restarted(replacementBus, *active, *legacy,
                                      profileDefaults, storage);
    QVERIFY(restarted.start().ok());
    QCOMPARE(restarted.revision(), quint64(0));
    {
        QtSettingsTransport transport(clientBus);
        SettingsClient client(transport, {QStringLiteral("services.doNotDisturb")}, timing);
        NotificationInterruptionPolicy policy;
        NotificationQuietingSettingsBridge bridge(client, policy);
        QSignalSpy policyChanges(&policy,
                                 &NotificationInterruptionPolicy::doNotDisturbEnabledChanged);
        QVERIFY(policy.doNotDisturbEnabled());
        QVERIFY(!bridge.controller().hasBaseline());
        QCOMPARE(policyChanges.size(), 0);
        QVERIFY(client.start(&error));
        QTRY_VERIFY_WITH_TIMEOUT(bridge.controller().ready()
                                     && bridge.controller().hasBaseline(), 2'000);
        QVERIFY(!policy.doNotDisturbEnabled());
        QCOMPARE(client.snapshot()->sourceLayers
                     .value(QStringLiteral("services.doNotDisturb")).toString(),
                 QStringLiteral("user-overrides"));
        QCOMPARE(policyChanges.size(), 1);
        QCOMPARE(restarted.revision(), quint64(0)); // disk restore, never replay
    }

    restarted.stop();
    QDBusConnection::disconnectFromBus(serviceConnection);
    QDBusConnection::disconnectFromBus(replacementConnection);
    QDBusConnection::disconnectFromBus(clientConnection);
    daemon.kill();
    QVERIFY(daemon.waitForFinished());
}

void NotificationQuietingSettingsBridgeTests::fixedLauncherExposesOnlyTheNotificationsRoute()
{
    int launches = 0;
    SettingsRouteLauncher launcher([&launches](QString *) { ++launches; return true; });
    QVERIFY(launcher.openNotifications());
    QCOMPARE(launches, 1);
    QVERIFY(launcher.errorText().isEmpty());
}

QTEST_GUILESS_MAIN(NotificationQuietingSettingsBridgeTests)
#include "tst_notificationquietingsettingsbridge.moc"
