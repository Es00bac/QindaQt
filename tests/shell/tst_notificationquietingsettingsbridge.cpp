// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationquietingsettingsbridge.h"
#include "settingsroutelauncher.h"

#include "qindaqt/services/notification_presentation_policy/notification_interruption_policy.h"
#include "qindaqt/services/notification_presentation_policy/notification_privacy_policy.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QtTest>

using namespace QindaQt::Shell;
using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::NotificationPresentationPolicy;
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
