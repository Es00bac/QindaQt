// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"
#include "taskorderpersistence.h"

#include <QSignalSpy>
#include <QtTest>

#include "task_list_applet_test_fakes.h"
#include "task_list_operation_test_support.h"
#include "task_list_test_support.h"

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskListApplet;
using namespace QindaQt::Shell;
using namespace QindaQt::Services::SettingsClient;
using TaskListOperationTest::FakeOperationAuthority;
using TaskListAppletTest::FakeTaskListOperationPort;
using QindaQt::Services::SettingsClient::SettingsClient;
using QindaQt::Services::SettingsClient::SettingsTransport;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

class FakeSettingsTransport final : public SettingsTransport {
    Q_OBJECT
public:
    bool start(QString *error) override
    {
        if (error != nullptr) {
            error->clear();
        }
        return true;
    }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner,
                         const QStringList &keys) override
    {
        snapshots.append({token, owner, keys});
    }
    void commit(quint64 token, const QString &owner, const QString &epoch,
                quint64 revision, const QVariantList &operations) override
    {
        commits.append({token, owner, epoch, revision, operations});
    }
    void requestActivation() override {}

    struct SnapshotRequest { quint64 token; QString owner; QStringList keys; };
    struct CommitRequest { quint64 token; QString owner; QString epoch;
                           quint64 revision; QVariantList operations; };
    QList<SnapshotRequest> snapshots;
    QList<CommitRequest> commits;
};

namespace {

QVariantMap snapshotWire(QString epoch, quint64 revision,
                         QVariantMap values)
{
    QVariantMap layers;
    for (auto it = values.begin(); it != values.end(); ++it) {
        layers.insert(it.key(), QStringLiteral("user-overrides"));
    }
    return {{QLatin1StringView(WireContract::FieldStatus),
             quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion),
             quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), std::move(epoch)},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), std::move(values)},
            {QLatin1StringView(WireContract::FieldSourceLayers),
             std::move(layers)},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

quint64 publishReady(TaskListSource &source, FakeOperationAuthority &authority)
{
    const auto evaluation = source.publishGeneration(
        {TaskListTest::standalone(QStringLiteral("w1"), QStringLiteral("app.1")),
         TaskListTest::standalone(QStringLiteral("w2"), QStringLiteral("app.2")),
         TaskListTest::standalone(QStringLiteral("w3"), QStringLiteral("app.3"))});
    if (!evaluation.ok()) {
        return 0;
    }
    authority.revision = source.revision();
    authority.sourceStatus = TaskListSourceStatus::Ready;
    authority.owner = QStringLiteral(":1.1");
    Q_EMIT authority.stateChanged();
    return source.revision();
}

} // namespace

class TaskOrderPersistenceTests final : public QObject {
    Q_OBJECT

private slots:
    void decodeHandlesAbsentMalformedAndValidValues();
    void mergePreservesUnrelatedKeys();
    void snapshotFeedsTheControllerAndEchoesAreNoOps();
    void userDragWritesMergedOrderToSettings();

private:
    static constexpr auto kKey = TaskOrderPersistence::kKey;
};

void TaskOrderPersistenceTests::decodeHandlesAbsentMalformedAndValidValues() {
    // Absent values decode to an empty order; malformed ones are rejected as
    // a whole so a hostile snapshot can never trust a partial order.
    const QStringList empty = TaskOrderPersistence::decodeTaskOrder(QVariant{})
                                  .value_or(QStringList{});
    QVERIFY(empty.isEmpty());
    QVERIFY(TaskOrderPersistence::decodeTaskOrder(
                QVariant(QStringLiteral("not-an-object")))
                .has_value() == false);
    QVERIFY(TaskOrderPersistence::decodeTaskOrder(
                QVariantMap{{QStringLiteral("taskOrder"), 42}})
                .has_value() == false);
    QVERIFY(TaskOrderPersistence::decodeTaskOrder(
                QVariantMap{{QStringLiteral("taskOrder"),
                             QVariantList{QStringLiteral("w1"), 7}}})
                .has_value() == false);
    QCOMPARE(TaskOrderPersistence::decodeTaskOrder(
                 QVariantMap{{QStringLiteral("taskOrder"),
                              QStringList{QStringLiteral("w2"),
                                          QStringLiteral("w1")}}})
                 .value_or(QStringList{}),
             (QStringList{QStringLiteral("w2"), QStringLiteral("w1")}));
}

void TaskOrderPersistenceTests::mergePreservesUnrelatedKeys() {
    const QVariantMap current{
        {QStringLiteral("some-panel"), QVariantMap{{QStringLiteral("opacity"), 0.5}}},
        {QStringLiteral("taskOrder"), QStringList{QStringLiteral("w-old")}}};
    const QVariantMap merged = TaskOrderPersistence::mergeTaskOrder(
        current, {QStringLiteral("w2"), QStringLiteral("w1")});
    QCOMPARE(merged.value(QStringLiteral("taskOrder")),
             QVariant(QStringList{QStringLiteral("w2"), QStringLiteral("w1")}));
    QCOMPARE(merged.value(QStringLiteral("some-panel")),
             current.value(QStringLiteral("some-panel")));
}

void TaskOrderPersistenceTests::snapshotFeedsTheControllerAndEchoesAreNoOps() {
    TaskListSource source;
    FakeOperationAuthority authority;
    FakeTaskListOperationPort port;
    TaskListAppletController controller(source, authority, port, {true, true, true});
    QVERIFY(publishReady(source, authority) > 0);

    FakeSettingsTransport transport;
    SettingsClient settings(transport,
                            QStringList{QLatin1StringView(kKey)},
                            {.requestTimeoutMilliseconds = 100,
                             .debounceMilliseconds = 0,
                             .retryMilliseconds = {10}});
    QVERIFY(settings.start());
    Q_EMIT transport.activationCompleted();
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.10"));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(
        request.token, request.owner,
        snapshotWire(QStringLiteral("epoch-a"), 7,
                     QVariantMap{{QLatin1StringView(kKey),
                                  QVariantMap{{QStringLiteral("taskOrder"),
                                               QStringList{QStringLiteral("w2")}}}}}));
    QTRY_VERIFY(settings.state() == ClientState::Ready);

    TaskOrderPersistence persistence(settings, controller);
    persistence.start();
    QTRY_COMPARE(controller.userTaskOrder(),
                 (QStringList{QStringLiteral("w2")}));
    // The displayed order follows the persisted overlay.
    QCOMPARE(controller.entryRows().at(0).toMap().value(QStringLiteral("taskId")),
             QVariant(QStringLiteral("w2")));

    // A settings echo of the same order must not renotify or reproject.
    QSignalSpy changedSpy(&controller,
                          &TaskListAppletController::userTaskOrderChanged);
    Q_EMIT transport.snapshotReceived(
        request.token, request.owner,
        snapshotWire(QStringLiteral("epoch-a"), 8,
                     QVariantMap{{QLatin1StringView(kKey),
                                  QVariantMap{{QStringLiteral("taskOrder"),
                                               QStringList{QStringLiteral("w2")}}}}}));
    QTRY_COMPARE(changedSpy.size(), 0);
}

void TaskOrderPersistenceTests::userDragWritesMergedOrderToSettings() {
    TaskListSource source;
    FakeOperationAuthority authority;
    FakeTaskListOperationPort port;
    TaskListAppletController controller(source, authority, port, {true, true, true});
    QVERIFY(publishReady(source, authority) > 0);

    FakeSettingsTransport transport;
    SettingsClient settings(transport,
                            QStringList{QLatin1StringView(kKey)},
                            {.requestTimeoutMilliseconds = 100,
                             .debounceMilliseconds = 0,
                             .retryMilliseconds = {10}});
    QVERIFY(settings.start());
    Q_EMIT transport.activationCompleted();
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.10"));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(
        request.token, request.owner,
        snapshotWire(QStringLiteral("epoch-a"), 7,
                     QVariantMap{{QLatin1StringView(kKey),
                                  QVariantMap{{QStringLiteral("some-panel"),
                                               QVariantMap{{QStringLiteral("dockZoom"),
                                                            false}}}}}}));
    QTRY_VERIFY(settings.state() == ClientState::Ready);

    TaskOrderPersistence persistence(settings, controller);
    persistence.start();

    // A user drag commits through the controller and must arrive as one
    // merged panels.configuration write that preserves unrelated keys.
    QVERIFY(controller.reorderTask(QStringLiteral("w3"),
                                   QStringLiteral("w2"), source.revision()));
    QTRY_COMPARE(transport.commits.size(), 1);
    const auto committed = transport.commits.takeFirst();
    QCOMPARE(committed.operations.size(), 1);
    const QVariantMap operation = committed.operations.at(0).toMap();
    QCOMPARE(operation.value(QLatin1StringView(WireContract::FieldKey)),
             QVariant(QLatin1StringView(kKey)));
    const QVariant written = operation.value(QLatin1StringView(WireContract::FieldValue));
    QCOMPARE(written.toMap().value(QStringLiteral("some-panel")),
             QVariant(QVariantMap{{QStringLiteral("dockZoom"), false}}));
    QCOMPARE(written.toMap().value(QStringLiteral("taskOrder")),
             QVariant(QStringList{QStringLiteral("w1"), QStringLiteral("w3"),
                                  QStringLiteral("w2")}));
}

QTEST_GUILESS_MAIN(TaskOrderPersistenceTests)
#include "tst_task_order_persistence.moc"
