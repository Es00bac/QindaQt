// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/removable_media_client/media_client.h>
#include "media_exporter.h"
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

namespace Public = QindaQt::RemovableMedia;
namespace Owner = QindaQt::Apps::RemovableMedia;
class ActionLauncher final : public Public::MediaOwnerLauncher {
public:
    int attempts = 0;
    bool startOwner() override { ++attempts; return false; }
};
class ActionBackend final : public Owner::MediaBackend {
public:
    QVector<Owner::Volume> items;
    QVector<Owner::Request> requests;
    QString root;
    bool pending = false;
    QVector<Owner::Volume> volumes() const override { return items; }
    bool available() const override { return true; }
    bool busy() const override { return pending; }
    QString pendingDriveIdentity() const override { return pending ? QStringLiteral("drive") : QString{}; }
    Public::ProgressPhase phase() const override { return pending ? Public::ProgressPhase::Mounting : Public::ProgressPhase::Idle; }
    QString diagnostic() const override { return {}; }
    QStringList formatTypes() const override { return {}; }
    void refresh() override { Q_EMIT changed(); }
    void execute(const Owner::Request &request) override {
        requests.append(request); pending = true; Q_EMIT changed();
    }
    void finish(Public::OperationStatus status, bool converged = true) {
        const auto request = requests.constLast();
        if (status == Public::OperationStatus::Applied && converged) {
            if (request.operation == Owner::Operation::Remove) items.clear();
            else for (auto &item : items) if (item.token == request.token) {
                if (request.operation == Owner::Operation::Unmount) { item.mountRoots.clear(); item.mountPath.clear(); }
                else {
                    item.mountPath = root; item.mountRoots = {root}; item.readOnlyKnown = true;
                    item.readOnly = request.operation == Owner::Operation::MountReadOnly;
                }
            }
        }
        Q_EMIT changed();
        pending = false;
        const auto mode = request.operation == Owner::Operation::Remove && status == Public::OperationStatus::Applied
            ? Public::RemovalMode::PoweredOff : Public::RemovalMode::None;
        const Owner::BackendCompletion completion{request.token, request.operation, status, mode,
            QStringLiteral("private message not exported"), status == Public::OperationStatus::Applied ? root : QString{}};
        Q_EMIT operationCompleted(completion);
        Q_EMIT finished(request.token, status == Public::OperationStatus::Applied, completion.message, completion.mountPath);
        Q_EMIT changed();
    }
};
struct Fixture final {
    QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("action-media-owner"));
    ActionBackend backend;
    QTemporaryDir directory;
    Owner::MediaPreferences preferences{directory.filePath(QStringLiteral("choices.json"))};
    Owner::MediaController controller{backend, preferences, false};
    Owner::MediaExporter exporter{backend, controller, bus};
    ActionLauncher launcher;
    std::unique_ptr<Public::MediaClient> client;
    Fixture() {
        Owner::Volume item;
        item.token = QStringLiteral("private-attachment");
        item.driveIdentity = QStringLiteral("drive");
        item.identity = QStringLiteral("filesystem");
        item.label = QStringLiteral("Fixture media");
        item.kind = QStringLiteral("USB storage");
        item.mountable = item.canMountReadOnly = true;
        backend.root = directory.path();
        backend.items = {item};
        backend.refresh();
    }
    ~Fixture() {
        client.reset();
        bus.unregisterService(QString::fromLatin1(Public::kServiceName));
        QDBusConnection::disconnectFromBus(QStringLiteral("action-media-owner"));
    }
    bool start() {
        if (!directory.isValid() || !exporter.publishObject()
            || !bus.registerService(QString::fromLatin1(Public::kServiceName))) return false;
        client = std::make_unique<Public::MediaClient>(QDBusConnection::sessionBus(), launcher);
        client->start();
        return true;
    }
    Public::ActionRequest request(Public::Action action, const QString &id) const {
        const auto snapshot = exporter.snapshot();
        return {Public::kProtocolVersion, id, snapshot.lineage, snapshot.rows.constFirst().attachment, action};
    }
};
class MediaActions final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void mountAndUnmountAwaitSameAttachmentReadback();
    void duplicateIdsAndGlobalBusyNeverRepeatMutation();
    void revokedAttachmentCannotUseReusedPrivatePath();
    void boundedRecentResultsNeverEvictPendingAdmission();
    void ownerLossAndDestructionNeverReplay();
    void contradictoryAppliedMountIsUncertain();
};
void MediaActions::mountAndUnmountAwaitSameAttachmentReadback()
{
    Fixture f; QVERIFY(f.start());
    QTRY_COMPARE(f.client->snapshot().availability, Public::Availability::Ready);
    QVector<Public::OperationResult> results;
    connect(f.client.get(), &Public::MediaSource::operationFinished, this, [&](const auto &r) { results.append(r); });
    const auto id = f.client->requestAction(f.client->snapshot().rows.first().attachment, Public::Action::MountReadOnly);
    QVERIFY(!id.isEmpty()); QVERIFY(f.client->actionPending());
    QTRY_COMPARE(f.backend.requests.size(), 1);
    QCOMPARE(f.client->snapshot().rows.first().mountState, Public::MountState::Unmounted);
    QVERIFY(results.isEmpty());
    f.backend.finish(Public::OperationStatus::Applied);
    QTRY_COMPARE(results.size(), 1);
    QCOMPARE(results.first().status, Public::OperationStatus::Applied);
    QVERIFY(results.first().confirmingRevision.has_value());
    QCOMPARE(f.client->snapshot().rows.first().preferredRoot, f.directory.path());
    QCOMPARE(f.client->snapshot().rows.first().readOnly, Public::ReadOnlyState::ReadOnly);
    QVERIFY(!f.client->actionPending());
    QTRY_VERIFY(f.client->snapshot().rows.first().actions.unmount.enabled);
    QVERIFY(!f.client->requestAction(f.client->snapshot().rows.first().attachment, Public::Action::Unmount).isEmpty());
    QTRY_COMPARE(f.backend.requests.size(), 2);
    QCOMPARE(results.size(), 1);
    f.backend.finish(Public::OperationStatus::Applied);
    QTRY_COMPARE(results.size(), 2);
    QCOMPARE(results.last().status, Public::OperationStatus::Applied);
    QCOMPARE(f.client->snapshot().rows.first().mountState, Public::MountState::Unmounted);
    QCOMPARE(f.launcher.attempts, 0);
}
void MediaActions::duplicateIdsAndGlobalBusyNeverRepeatMutation()
{
    Fixture f;
    auto request = f.request(Public::Action::Mount, QStringLiteral("one-request"));
    auto first = f.exporter.admit(request, QStringLiteral(":1.200"));
    QCOMPARE(first.status, Public::AdmissionStatus::Accepted);
    QCOMPARE(f.backend.requests.size(), 1);
    QCOMPARE(f.exporter.admit(request, QStringLiteral(":1.200")), first);
    QCOMPARE(f.backend.requests.size(), 1);
    auto changed = request; changed.action = Public::Action::Remove;
    QCOMPARE(f.exporter.admit(changed, QStringLiteral(":1.200")).status, Public::AdmissionStatus::Invalid);
    auto busy = f.request(Public::Action::Remove, QStringLiteral("next-request"));
    QCOMPARE(f.exporter.admit(busy, QStringLiteral(":1.201")).status, Public::AdmissionStatus::Busy);
    f.backend.finish(Public::OperationStatus::Applied);
    QCOMPARE(f.exporter.admit(request, QStringLiteral(":1.200")), first);
    QCOMPARE(f.backend.requests.size(), 1);
}
void MediaActions::revokedAttachmentCannotUseReusedPrivatePath()
{
    Fixture f;
    const auto old = f.request(Public::Action::Mount, QStringLiteral("old-request"));
    f.backend.items[0].token = QStringLiteral("new-attachment-same-private-path");
    f.backend.refresh();
    auto fresh = old; fresh.lineage = f.exporter.snapshot().lineage;
    QCOMPARE(f.exporter.admit(fresh, QStringLiteral(":1.202")).status, Public::AdmissionStatus::Gone);
    auto stale = old; stale.requestId = QStringLiteral("distinct-stale-request");
    QCOMPARE(f.exporter.admit(stale, QStringLiteral(":1.202")).status, Public::AdmissionStatus::Stale);
    QVERIFY(f.backend.requests.isEmpty());
}
void MediaActions::boundedRecentResultsNeverEvictPendingAdmission()
{
    Fixture f;
    for (qsizetype i = 0; i < Public::kMaxRecentRequests; ++i) {
        const auto request = f.request(Public::Action::ShowDetails, QStringLiteral("details_%1").arg(i));
        QCOMPARE(f.exporter.admit(request, QStringLiteral(":1.203")).status, Public::AdmissionStatus::Accepted);
    }
    const auto next = f.request(Public::Action::Mount, QStringLiteral("over-bound"));
    QCOMPARE(f.exporter.admit(next, QStringLiteral(":1.203")).status, Public::AdmissionStatus::Busy);
    QVERIFY(f.backend.requests.isEmpty());
}
void MediaActions::ownerLossAndDestructionNeverReplay()
{
    Fixture f; QVERIFY(f.start());
    QTRY_COMPARE(f.client->snapshot().availability, Public::Availability::Ready);
    QVector<Public::OperationResult> results;
    connect(f.client.get(), &Public::MediaSource::operationFinished, this, [&](const auto &r) { results.append(r); });
    QVERIFY(!f.client->requestAction(f.client->snapshot().rows.first().attachment, Public::Action::Mount).isEmpty());
    QTRY_COMPARE(f.backend.requests.size(), 1);
    QVERIFY(f.bus.unregisterService(QString::fromLatin1(Public::kServiceName)));
    QTRY_COMPARE(results.size(), 1);
    QCOMPARE(results.first().status, Public::OperationStatus::Uncertain);
    f.backend.finish(Public::OperationStatus::Applied);
    QVERIFY(f.bus.registerService(QString::fromLatin1(Public::kServiceName)));
    QTRY_COMPARE(f.client->snapshot().availability, Public::Availability::Ready);
    QCOMPARE(results.size(), 1); QCOMPARE(f.backend.requests.size(), 1);
    QTRY_VERIFY(f.client->snapshot().rows.first().actions.unmount.enabled);
    QVERIFY(!f.client->requestAction(f.client->snapshot().rows.first().attachment, Public::Action::Unmount).isEmpty());
    QTRY_COMPARE(f.backend.requests.size(), 2);
    f.client.reset();
    QVERIFY(f.backend.pending);
    f.backend.finish(Public::OperationStatus::Applied);
    QCOMPARE(f.backend.requests.size(), 2);
}
void MediaActions::contradictoryAppliedMountIsUncertain()
{
    Fixture f; QVERIFY(f.start());
    QTRY_COMPARE(f.client->snapshot().availability, Public::Availability::Ready);
    QVector<Public::OperationResult> results;
    connect(f.client.get(), &Public::MediaSource::operationFinished, this, [&](const auto &r) { results.append(r); });
    QVERIFY(!f.client->requestAction(f.client->snapshot().rows.first().attachment, Public::Action::Mount).isEmpty());
    QTRY_COMPARE(f.backend.requests.size(), 1);
    f.backend.finish(Public::OperationStatus::Applied, false);
    QTRY_COMPARE(results.size(), 1);
    QCOMPARE(results.first().status, Public::OperationStatus::Uncertain);
    QCOMPARE(f.backend.requests.size(), 1);
}
QTEST_GUILESS_MAIN(MediaActions)
#include "tst_media_actions.moc"
