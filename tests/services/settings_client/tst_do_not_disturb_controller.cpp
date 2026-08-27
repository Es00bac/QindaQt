// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/settings_client/do_not_disturb_controller.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QtTest>

#include <limits>

using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

namespace {

class ControllerTransport final : public SettingsTransport {
    Q_OBJECT
public:
    bool start(QString *) override { return true; }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner, const QStringList &) override
    {
        snapshots.append({token, owner});
    }
    void commit(quint64 token, const QString &owner, const QString &epoch,
                quint64 revision, const QVariantList &) override
    {
        commits.append({token, owner, epoch, revision});
    }
    void requestActivation() override {}

    struct SnapshotRequest { quint64 token; QString owner; };
    struct CommitRequest { quint64 token; QString owner; QString epoch; quint64 revision; };
    QList<SnapshotRequest> snapshots;
    QList<CommitRequest> commits;
};

QVariantMap snapshotWire(QString epoch, quint64 revision, bool enabled)
{
    const QString key = QStringLiteral("services.doNotDisturb");
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), std::move(epoch)},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), QVariantMap{{key, enabled}}},
            {QLatin1StringView(WireContract::FieldSourceLayers),
             QVariantMap{{key, QStringLiteral("user-overrides")}}},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

QVariantMap commitWire(SettingsWireStatus status, quint64 before, quint64 after,
                       bool enabled, QString epoch, QString message = {})
{
    const QString key = QStringLiteral("services.doNotDisturb");
    const QStringList changed = status == SettingsWireStatus::Applied && after == before + 1
        ? QStringList{key} : QStringList{};
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(status)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), std::move(epoch)},
            {QLatin1StringView(WireContract::FieldRevisionBefore), before},
            {QLatin1StringView(WireContract::FieldRevisionAfter), after},
            {QLatin1StringView(WireContract::FieldValues), QVariantMap{{key, enabled}}},
            {QLatin1StringView(WireContract::FieldSourceLayers),
             QVariantMap{{key, QStringLiteral("user-overrides")}}},
            {QLatin1StringView(WireContract::FieldChangedKeys), changed},
            {QLatin1StringView(WireContract::FieldMessage), std::move(message)}};
}

void establishBaseline(ControllerTransport &transport, const QString &owner,
                       const QString &epoch, quint64 revision, bool enabled)
{
    Q_EMIT transport.ownerChanged(owner);
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(epoch, revision, enabled));
}

} // namespace

class DoNotDisturbControllerTests final : public QObject {
    Q_OBJECT
private slots:
    void ownerLossDominatesAcceptedSaveAndConflict();
    void retainsConfirmedFailures_data();
    void retainsConfirmedFailures();
};

void DoNotDisturbControllerTests::ownerLossDominatesAcceptedSaveAndConflict()
{
    ControllerTransport transport;
    SettingsClient client(transport, {QStringLiteral("services.doNotDisturb")},
                          {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    DoNotDisturbController controller(client);
    QVERIFY(client.start());
    establishBaseline(transport, QStringLiteral(":1.31"),
                      QStringLiteral("epoch-a"), 3, false);
    QTRY_VERIFY(controller.ready());

    QVERIFY(controller.requestSet(true));
    auto commit = transport.commits.constLast();
    Q_EMIT transport.commitReceived(
        commit.token, commit.owner,
        commitWire(SettingsWireStatus::Applied, 3, 4, true,
                   QStringLiteral("epoch-a")));
    QVERIFY(controller.unavailable());
    QVERIFY(!controller.enabled());
    Q_EMIT transport.ownerChanged(QString{});
    QVERIFY(controller.unavailable());
    QCOMPARE(transport.commits.size(), 1);

    establishBaseline(transport, QStringLiteral(":1.32"),
                      QStringLiteral("epoch-b"), 0, true);
    QTRY_VERIFY(controller.ready());
    QVERIFY(controller.requestSet(false));
    commit = transport.commits.constLast();
    Q_EMIT transport.commitReceived(
        commit.token, commit.owner,
        commitWire(SettingsWireStatus::Conflict, 1, 1, true,
                   QStringLiteral("epoch-b")));
    QVERIFY(controller.unavailable());
    Q_EMIT transport.ownerChanged(QString{});
    QVERIFY(controller.unavailable());
    QVERIFY(!controller.applyMyChoice());

    establishBaseline(transport, QStringLiteral(":1.33"),
                      QStringLiteral("epoch-c"), 0, true);
    QTRY_VERIFY(controller.conflict());
    QCOMPARE(transport.commits.size(), 2);
    QVERIFY(controller.applyMyChoice());
    QCOMPARE(transport.commits.size(), 3);
}

void DoNotDisturbControllerTests::retainsConfirmedFailures_data()
{
    QTest::addColumn<quint32>("status");
    QTest::addColumn<quint64>("revision");
    QTest::addColumn<QString>("message");
    QTest::newRow("persistence-failed")
        << quint32(SettingsWireStatus::PersistenceFailed) << quint64(3)
        << QStringLiteral("durable save failed");
    QTest::newRow("validation-failed")
        << quint32(SettingsWireStatus::ValidationFailed) << quint64(3)
        << QStringLiteral("value was rejected");
    QTest::newRow("revision-exhausted")
        << quint32(SettingsWireStatus::RevisionExhausted)
        << std::numeric_limits<quint64>::max()
        << QStringLiteral("revision counter is exhausted");
}

void DoNotDisturbControllerTests::retainsConfirmedFailures()
{
    QFETCH(quint32, status);
    QFETCH(quint64, revision);
    QFETCH(QString, message);
    ControllerTransport transport;
    SettingsClient client(transport, {QStringLiteral("services.doNotDisturb")},
                          {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    DoNotDisturbController controller(client);
    QVERIFY(client.start());
    establishBaseline(transport, QStringLiteral(":1.34"),
                      QStringLiteral("epoch"), revision, false);
    QTRY_VERIFY(controller.ready());
    QVERIFY(controller.requestSet(true));
    const auto commit = transport.commits.constLast();
    Q_EMIT transport.commitReceived(
        commit.token, commit.owner,
        commitWire(SettingsWireStatus(status), revision, revision, false,
                   QStringLiteral("epoch"), message));
    QVERIFY(controller.unavailable());
    QCOMPARE(controller.errorText(), message);

    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto refresh = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(
        refresh.token, refresh.owner,
        snapshotWire(QStringLiteral("epoch"), revision, false));
    QTRY_VERIFY(controller.ready());
    QCOMPARE(controller.errorText(), message);
    QCOMPARE(transport.commits.size(), 1);

    QVERIFY(controller.requestSet(true));
    QCOMPARE(controller.errorText(), QString{});
    QCOMPARE(transport.commits.size(), 2);
}

QTEST_GUILESS_MAIN(DoNotDisturbControllerTests)
#include "tst_do_not_disturb_controller.moc"
