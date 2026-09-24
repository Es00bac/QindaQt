// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_notifications/notification_application_settings_model.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QTimer>
#include <QtTest>

using QindaQt::Apps::SettingsNotifications::NotificationApplicationSettingsModel;
using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::NotificationPresentationPolicy;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

namespace {
const QString PoliciesKey = QStringLiteral("services.notificationPolicies");
const QString ApplicationId = QStringLiteral("org.example.PolicyTest");
const QString Owner = QStringLiteral(":1.71");
const QString ReplacementOwner = QStringLiteral(":1.72");
const QString Epoch = QStringLiteral("notification-epoch-a");
const QString ReplacementEpoch = QStringLiteral("notification-epoch-b");

using PolicyMap = PerApplicationNotificationPolicies;

PolicyMap originalPolicies()
{
    return {{ApplicationId, {.muted = true, .soundEnabled = true}}};
}

PolicyMap requestedPolicies()
{
    return {{ApplicationId, {.muted = false, .soundEnabled = true}}};
}

QVariantMap encodedPolicies(const PolicyMap &policies)
{
    QVariantMap result;
    QString error;
    const bool encoded = NotificationApplicationPolicy::encodeSettingsValue(
        policies, &result, &error);
    Q_ASSERT(encoded);
    Q_ASSERT(error.isEmpty());
    return result;
}

QVariantMap sourcesFor(const QVariantMap &values)
{
    QVariantMap sources;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        sources.insert(it.key(), QStringLiteral("system-defaults"));
    }
    return sources;
}

QVariantMap snapshotWire(quint64 revision, const PolicyMap &policies,
                         const QString &epoch = Epoch)
{
    const QVariantMap values{{PoliciesKey, encodedPolicies(policies)}};
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), epoch},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sourcesFor(values)},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

QVariantMap appliedCommitWire(quint64 before, quint64 after,
                              const QVariantMap &policies)
{
    const QVariantMap values{{PoliciesKey, policies}};
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), Epoch},
            {QLatin1StringView(WireContract::FieldRevisionBefore), before},
            {QLatin1StringView(WireContract::FieldRevisionAfter), after},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sourcesFor(values)},
            {QLatin1StringView(WireContract::FieldChangedKeys), QStringList{PoliciesKey}},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

class FakeTransport final : public SettingsTransport {
public:
    struct SnapshotRequest final {
        quint64 token = 0;
        QString owner;
    };
    struct CommitRequest final {
        quint64 token = 0;
        QString owner;
        QVariantList operations;
    };

    bool start(QString *) override { return true; }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner,
                         const QStringList &) override
    {
        snapshots.append({token, owner});
    }
    void commit(quint64 token, const QString &owner, const QString &,
                quint64, const QVariantList &operations) override
    {
        commits.append({token, owner, operations});
    }
    void requestActivation() override {}

    QList<SnapshotRequest> snapshots;
    QList<CommitRequest> commits;
};

struct Harness final {
    FakeTransport transport;
    SettingsClient client{transport, {PoliciesKey},
                          {.requestTimeoutMilliseconds = 800,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}}};
    NotificationApplicationSettingsModel model{
        client, {{ApplicationId, QStringLiteral("Policy Test App"),
                  QStringLiteral("application-x-executable")}}};

    bool establish()
    {
        if (!client.start()) {
            return false;
        }
        Q_EMIT transport.ownerChanged(Owner);
        return answerNext(3, originalPolicies()) && model.available();
    }

    bool answerNext(quint64 revision, const PolicyMap &policies)
    {
        if (!QTest::qWaitFor([this] { return !transport.snapshots.isEmpty(); }, 1'000)) {
            return false;
        }
        const auto request = transport.snapshots.takeFirst();
        if (request.owner != Owner) {
            return false;
        }
        Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                          snapshotWire(revision, policies));
        return true;
    }

    bool submitAndApply()
    {
        if (!model.requestSetMuted(ApplicationId, false) ||
            transport.commits.size() != 1) {
            return false;
        }
        const auto request = transport.commits.constFirst();
        const QVariantMap operation = request.operations.constFirst().toMap();
        const QVariantMap value = operation.value(
            QLatin1StringView(WireContract::FieldValue)).toMap();
        Q_EMIT transport.commitReceived(
            request.token, request.owner,
            appliedCommitWire(3, 4, value));
        return true;
    }
};
} // namespace

class NotificationApplicationOutcomeTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void lowerRevisionReadbackWaitsForAppliedFloor();
    void permanentBelowFloorReadbackTimesOutWithoutReplay();
    void ownerAndEpochLossRetireAppliedWriteWithoutReplay();
};

void NotificationApplicationOutcomeTests::lowerRevisionReadbackWaitsForAppliedFloor()
{
    Harness h;
    QVERIFY(h.establish());
    QVERIFY(h.submitAndApply());
    QVERIFY(h.model.pending());
    QVERIFY(h.answerNext(3, originalPolicies()));

    QVERIFY(h.model.pending());
    QVERIFY(!h.model.uncertain());
    QVERIFY(!h.model.conflict());
    QVERIFY(h.model.data(h.model.index(0),
        NotificationApplicationSettingsModel::MutedRole).toBool());
    QCOMPARE(h.transport.commits.size(), 1);

    QVERIFY(h.answerNext(4, requestedPolicies()));
    QTRY_VERIFY(!h.model.pending());
    QVERIFY(!h.model.uncertain());
    QVERIFY(!h.model.conflict());
    QVERIFY(!h.model.data(h.model.index(0),
        NotificationApplicationSettingsModel::MutedRole).toBool());
    QCOMPARE(h.transport.commits.size(), 1);
}

void NotificationApplicationOutcomeTests::permanentBelowFloorReadbackTimesOutWithoutReplay()
{
    Harness h;
    QVERIFY(h.establish());
    QVERIFY(h.submitAndApply());

    QTimer responder;
    responder.setInterval(5);
    connect(&responder, &QTimer::timeout, &h.model, [&h] {
        while (!h.transport.snapshots.isEmpty()) {
            const auto request = h.transport.snapshots.takeFirst();
            Q_EMIT h.transport.snapshotReceived(request.token, request.owner,
                                                snapshotWire(3, originalPolicies()));
        }
    });
    responder.start();

    QTRY_VERIFY_WITH_TIMEOUT(h.model.uncertain(), 6'000);
    responder.stop();
    QVERIFY(!h.model.pending());
    QVERIFY(!h.model.conflict());
    QVERIFY(h.model.errorText().contains(QStringLiteral("in time")));
    QVERIFY(h.model.data(h.model.index(0),
        NotificationApplicationSettingsModel::MutedRole).toBool());
    QCOMPARE(h.transport.commits.size(), 1);
}

void NotificationApplicationOutcomeTests::ownerAndEpochLossRetireAppliedWriteWithoutReplay()
{
    {
        Harness h;
        QVERIFY(h.establish());
        QVERIFY(h.submitAndApply());
        Q_EMIT h.transport.ownerChanged(ReplacementOwner);
        QVERIFY(!h.model.pending());
        QVERIFY(h.model.uncertain());
        QCOMPARE(h.transport.commits.size(), 1);

        QVERIFY(QTest::qWaitFor([&h] {
            return !h.transport.snapshots.isEmpty();
        }, 1'000));
        const auto request = h.transport.snapshots.takeFirst();
        QCOMPARE(request.owner, ReplacementOwner);
        Q_EMIT h.transport.snapshotReceived(request.token, request.owner,
            snapshotWire(1, originalPolicies()));
        QVERIFY(!h.model.pending());
        QVERIFY(h.model.uncertain());
        QCOMPARE(h.transport.commits.size(), 1);
    }

    {
        Harness h;
        QVERIFY(h.establish());
        QVERIFY(h.submitAndApply());
        QVERIFY(QTest::qWaitFor([&h] {
            return !h.transport.snapshots.isEmpty();
        }, 1'000));
        const auto request = h.transport.snapshots.takeFirst();
        Q_EMIT h.transport.snapshotReceived(request.token, request.owner,
            snapshotWire(4, requestedPolicies(), ReplacementEpoch));
        QVERIFY(!h.model.pending());
        QVERIFY(h.model.uncertain());
        QCOMPARE(h.transport.commits.size(), 1);
    }
}

QTEST_GUILESS_MAIN(NotificationApplicationOutcomeTests)
#include "tst_notification_application_outcomes.moc"
