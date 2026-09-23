// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/apps/settings/notifications/notification_schedule_model.h"
#include "qindaqt/services/settings_client/do_not_disturb_controller.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QtTest>

using QindaQt::Apps::SettingsNotifications::NotificationScheduleModel;
using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

namespace {
const QString Dnd = QStringLiteral("services.doNotDisturb");
const QString Schedule = QStringLiteral("services.doNotDisturbSchedule");
const QString Start = QStringLiteral("services.doNotDisturbStartMinutes");
const QString End = QStringLiteral("services.doNotDisturbEndMinutes");
const QString OwnerA = QStringLiteral(":1.71");
const QString EpochA = QStringLiteral("notification-epoch-a");

QVariantMap settingsValues(bool dnd = false, bool schedule = false,
                           int start = 1320, int end = 420)
{
    return {{Dnd, dnd}, {Schedule, schedule}, {Start, start}, {End, end}};
}

QVariantMap sourceLayers(const QVariantMap &values)
{
    QVariantMap sources;
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        sources.insert(it.key(), QStringLiteral("system-defaults"));
    return sources;
}

QVariantMap snapshotWire(const QString &epoch, quint64 revision,
                         const QVariantMap &values)
{
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), epoch},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sourceLayers(values)},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

QVariantMap commitWire(const QString &epoch, SettingsWireStatus status,
                       quint64 before, quint64 after, const QString &key,
                       const QVariant &value, const QString &message = {})
{
    const QVariantMap values{{key, value}};
    const QStringList changed = status == SettingsWireStatus::Applied && after > before
        ? QStringList{key} : QStringList{};
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(status)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), epoch},
            {QLatin1StringView(WireContract::FieldRevisionBefore), before},
            {QLatin1StringView(WireContract::FieldRevisionAfter), after},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sourceLayers(values)},
            {QLatin1StringView(WireContract::FieldChangedKeys), changed},
            {QLatin1StringView(WireContract::FieldMessage), message}};
}

class FakeTransport final : public SettingsTransport {
    Q_OBJECT
public:
    bool start(QString *) override { return true; }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner, const QStringList &) override
    { snapshots.append({token, owner}); }
    void commit(quint64 token, const QString &owner, const QString &, quint64 revision,
                const QVariantList &operations) override
    { commits.append({token, owner, revision, operations}); }
    void requestActivation() override {}

    struct SnapshotRequest { quint64 token; QString owner; };
    struct CommitRequest { quint64 token; QString owner; quint64 revision; QVariantList operations; };
    QList<SnapshotRequest> snapshots;
    QList<CommitRequest> commits;
};

struct Harness {
    FakeTransport transport;
    SettingsClient client{transport, {Dnd, Schedule, Start, End},
                          {.requestTimeoutMilliseconds = 400,
                           .debounceMilliseconds = 0, .retryMilliseconds = {10}}};
    NotificationScheduleModel schedule{client};
    DoNotDisturbController dnd{client};

    bool answer(const QString &owner, const QString &epoch, quint64 revision,
                const QVariantMap &values)
    {
        if (!QTest::qWaitFor([this] { return !transport.snapshots.isEmpty(); }, 2000))
            return false;
        const auto request = transport.snapshots.takeFirst();
        if (request.owner != owner) return false;
        Q_EMIT transport.snapshotReceived(request.token, owner,
                                          snapshotWire(epoch, revision, values));
        return true;
    }

    bool establish(const QString &owner = OwnerA, const QString &epoch = EpochA,
                   quint64 revision = 1, const QVariantMap &values = settingsValues())
    {
        if (!client.start()) return false;
        Q_EMIT transport.ownerChanged(owner);
        return answer(owner, epoch, revision, values) && schedule.available() && dnd.ready();
    }

    void reply(SettingsWireStatus status, quint64 before, quint64 after,
               const QString &key, const QVariant &value, const QString &message = {})
    {
        const auto commit = transport.commits.constLast();
        Q_EMIT transport.commitReceived(commit.token, commit.owner,
            commitWire(EpochA, status, before, after, key, value, message));
    }
};
} // namespace

class NotificationScheduleOutcomeTests final : public QObject {
    Q_OBJECT
private slots:
    void admissionAndConfirmedReadbackStayOnSchedule();
    void malformedScheduleDoesNotEnableEditor();
    void rejectionConflictAndUncertaintyStayOnSchedule();
    void ownerReplacementAndUnconfirmableReadbackNeverReplay();
    void dndSaveCannotBorrowScheduleResult();
};

void NotificationScheduleOutcomeTests::admissionAndConfirmedReadbackStayOnSchedule()
{
    Harness h;
    QVERIFY(!h.schedule.available());
    QVERIFY(!h.schedule.canEdit());
    QVERIFY(!h.dnd.canToggle());
    h.schedule.setScheduleEnabled(true);
    QVERIFY(h.transport.commits.isEmpty());
    QVERIFY(h.establish());
    QVERIFY(h.schedule.canEdit());
    h.client.refresh();
    QTRY_VERIFY(!h.transport.snapshots.isEmpty());
    // A same-owner refresh occupies the serial lane while the client remains
    // Ready; both controls must lose admission before an attempted edit.
    QVERIFY(!h.schedule.canEdit());
    QVERIFY(!h.dnd.canToggle());
    h.schedule.setEnd(8, 15);
    QVERIFY(!h.dnd.requestSet(true));
    QVERIFY(h.transport.commits.isEmpty());
    QVERIFY(!h.schedule.pending());
    QVERIFY(h.schedule.errorText().isEmpty());
    QVERIFY(h.answer(OwnerA, EpochA, 1, settingsValues()));
    QCOMPARE(h.schedule.endMinutes(), 420);
    QVERIFY(h.schedule.canEdit());
    QVERIFY(h.dnd.canToggle());
    h.schedule.setStart(21, 30);
    QCOMPARE(h.transport.commits.size(), 1);
    QCOMPARE(h.transport.commits.constLast().operations.constFirst().toMap()
                 .value(QLatin1StringView(WireContract::FieldKey)).toString(), Start);
    QVERIFY(h.schedule.pending());
    QVERIFY(!h.schedule.canEdit());
    QVERIFY(!h.dnd.canToggle());
    QVERIFY(!h.dnd.requestSet(true));
    h.reply(SettingsWireStatus::Applied, 1, 2, Start, 1290);
    QVERIFY(h.schedule.pending());
    QVERIFY(!h.dnd.saving());
    QVERIFY(h.dnd.errorText().isEmpty());
    QVERIFY(h.answer(OwnerA, EpochA, 2, settingsValues(false, false, 1290)));
    QVERIFY(!h.schedule.pending());
    QVERIFY(h.schedule.canEdit());
    QCOMPARE(h.schedule.startMinutes(), 1290);
    QVERIFY(h.dnd.ready());
    QCOMPARE(h.transport.commits.size(), 1);
    h.client.refresh();
    QVERIFY(h.answer(OwnerA, EpochA, 2, settingsValues(false, false, 1290)));
    QVERIFY(h.schedule.canEdit());
    QVERIFY(h.schedule.errorText().isEmpty());
    h.client.refresh();
    QVERIFY(h.answer(OwnerA, EpochA, 3, settingsValues(true, true, 1260)));
    QCOMPARE(h.schedule.startMinutes(), 1260);
    QVERIFY(h.schedule.scheduleEnabled());
    QVERIFY(h.dnd.enabled());
    QCOMPARE(h.transport.commits.size(), 1);
}

void NotificationScheduleOutcomeTests::malformedScheduleDoesNotEnableEditor()
{
    Harness h;
    QVERIFY(h.client.start());
    Q_EMIT h.transport.ownerChanged(OwnerA);
    QVariantMap values = settingsValues();
    values.insert(Start, QStringLiteral("1320"));
    QVERIFY(h.answer(OwnerA, EpochA, 1, values));
    QVERIFY(!h.schedule.available());
    QVERIFY(!h.schedule.canEdit());
    QVERIFY(h.dnd.ready());
    h.schedule.setStart(21, 30);
    QVERIFY(h.transport.commits.isEmpty());
}

void NotificationScheduleOutcomeTests::rejectionConflictAndUncertaintyStayOnSchedule()
{
    Harness h;
    QVERIFY(h.establish());
    h.schedule.setEnd(8, 15);
    QCOMPARE(h.transport.commits.size(), 1);
    h.reply(SettingsWireStatus::ReadOnlyLayer, 1, 1, End, 420,
            QStringLiteral("policy blocks this time"));
    QVERIFY(!h.schedule.pending());
    QCOMPARE(h.schedule.endMinutes(), 420);
    QVERIFY(h.schedule.errorText().contains(QStringLiteral("policy")));
    QVERIFY(h.dnd.errorText().isEmpty());
    QVERIFY(!h.dnd.conflict());
    QVERIFY(h.answer(OwnerA, EpochA, 1, settingsValues()));
    QVERIFY(h.schedule.errorText().contains(QStringLiteral("policy")));

    h.schedule.setScheduleEnabled(true);
    h.reply(SettingsWireStatus::Conflict, 2, 2, Schedule, false,
            QStringLiteral("changed elsewhere"));
    QVERIFY(h.schedule.conflict());
    QVERIFY(!h.schedule.pending());
    QVERIFY(!h.dnd.conflict());
    QVERIFY(h.answer(OwnerA, EpochA, 2, settingsValues()));
    QVERIFY(h.schedule.conflict());
    QVERIFY(!h.schedule.scheduleEnabled());

    h.schedule.setEnd(8, 15);
    const auto commit = h.transport.commits.constLast();
    Q_EMIT h.transport.requestFailed(commit.token, commit.owner,
                                     QStringLiteral("org.freedesktop.DBus.Error.NoReply"),
                                     QStringLiteral("reply lost"));
    QVERIFY(h.schedule.uncertain());
    QVERIFY(!h.schedule.pending());
    // The shared client is degraded, so DND is unavailable too, but it did
    // not consume this schedule failure as its own save result.
    QVERIFY(h.dnd.unavailable());
    QVERIFY(!h.dnd.saving());
    QVERIFY(!h.dnd.conflict());
    QVERIFY(h.answer(OwnerA, EpochA, 2, settingsValues()));
    QCOMPARE(h.transport.commits.size(), 3);
    QCOMPARE(h.schedule.endMinutes(), 420);
}

void NotificationScheduleOutcomeTests::ownerReplacementAndUnconfirmableReadbackNeverReplay()
{
    Harness h;
    QVERIFY(h.establish());
    h.schedule.setStart(21, 30);
    h.reply(SettingsWireStatus::Applied, 1, 2, Start, 1290);
    QVERIFY(h.schedule.pending());
    QSignalSpy changed(&h.schedule, &NotificationScheduleModel::viewChanged);
    Q_EMIT h.transport.ownerChanged(QStringLiteral(":1.72"));
    QVERIFY(!h.schedule.pending());
    QVERIFY(h.schedule.uncertain());
    QVERIFY(!h.schedule.canEdit());
    QVERIFY(changed.size() >= 1);
    QVERIFY(h.answer(QStringLiteral(":1.72"), QStringLiteral("notification-epoch-b"), 1,
                     settingsValues()));
    QVERIFY(h.schedule.canEdit());
    QCOMPARE(h.transport.commits.size(), 1);

    h.schedule.setEnd(8, 15);
    QCOMPARE(h.transport.commits.size(), 2);
    const auto commit = h.transport.commits.constLast();
    Q_EMIT h.transport.commitReceived(commit.token, commit.owner,
        commitWire(QStringLiteral("notification-epoch-b"), SettingsWireStatus::Applied,
                   1, 2, End, 495));
    QVERIFY(h.schedule.pending());
    QVERIFY(h.answer(QStringLiteral(":1.72"), QStringLiteral("notification-epoch-b"), 1,
                     settingsValues()));
    QVERIFY(!h.schedule.pending());
    QVERIFY(h.schedule.uncertain());
    QCOMPARE(h.schedule.endMinutes(), 420);
    QCOMPARE(h.transport.commits.size(), 2);

    Harness different;
    QVERIFY(different.establish());
    different.schedule.setEnd(8, 15);
    different.reply(SettingsWireStatus::Applied, 1, 2, End, 495);
    QVERIFY(different.answer(OwnerA, EpochA, 2, settingsValues(false, false, 1320, 480)));
    QVERIFY(different.schedule.conflict());
    QCOMPARE(different.schedule.endMinutes(), 480);
    QVERIFY(!different.dnd.conflict());
    QCOMPARE(different.transport.commits.size(), 1);

    Harness timeout;
    QVERIFY(timeout.establish());
    timeout.schedule.setEnd(8, 15);
    timeout.reply(SettingsWireStatus::Applied, 1, 2, End, 495);
    QTRY_VERIFY(!timeout.transport.snapshots.isEmpty());
    const auto refresh = timeout.transport.snapshots.takeFirst();
    Q_EMIT timeout.transport.requestFailed(refresh.token, refresh.owner,
        QStringLiteral("org.freedesktop.DBus.Error.NoReply"), QStringLiteral("readback lost"));
    QVERIFY(!timeout.schedule.pending());
    QVERIFY(timeout.schedule.uncertain());
    QCOMPARE(timeout.schedule.endMinutes(), 420);
    QCOMPARE(timeout.transport.commits.size(), 1);
}

void NotificationScheduleOutcomeTests::dndSaveCannotBorrowScheduleResult()
{
    Harness h;
    QVERIFY(h.establish());
    QVERIFY(h.dnd.requestSet(true));
    QVERIFY(!h.schedule.canEdit());
    QCOMPARE(h.transport.commits.size(), 1);
    h.reply(SettingsWireStatus::ReadOnlyLayer, 1, 1, Dnd, false,
            QStringLiteral("DND policy blocks this change"));
    QVERIFY(!h.schedule.pending());
    QVERIFY(h.schedule.errorText().isEmpty());
    QCOMPARE(h.dnd.errorText(), QStringLiteral("DND policy blocks this change"));
    QVERIFY(h.answer(OwnerA, EpochA, 1, settingsValues()));
    QCOMPARE(h.dnd.errorText(), QStringLiteral("DND policy blocks this change"));
    QVERIFY(h.schedule.canEdit());

    QVERIFY(h.dnd.requestSet(true));
    h.reply(SettingsWireStatus::Applied, 1, 2, Dnd, true);
    QVERIFY(h.answer(OwnerA, EpochA, 1, settingsValues()));
    QVERIFY(h.dnd.unavailable());
    QVERIFY(!h.dnd.enabled());
    QCOMPARE(h.transport.commits.size(), 2);

    Harness mismatch;
    QVERIFY(mismatch.establish());
    QVERIFY(mismatch.dnd.requestSet(true));
    mismatch.reply(SettingsWireStatus::Applied, 1, 2, Dnd, true);
    QVERIFY(mismatch.answer(OwnerA, EpochA, 2, settingsValues()));
    QVERIFY(mismatch.dnd.conflict());
    QVERIFY(!mismatch.dnd.enabled());
    mismatch.schedule.setEnd(8, 15);
    QCOMPARE(mismatch.transport.commits.size(), 2);
    QVERIFY(!mismatch.dnd.applyMyChoice());
    QVERIFY(mismatch.dnd.conflict());
    mismatch.reply(SettingsWireStatus::ReadOnlyLayer, 2, 2, End, 420);
    QVERIFY(mismatch.answer(OwnerA, EpochA, 2, settingsValues()));
    QVERIFY(mismatch.dnd.conflict());
    QVERIFY(mismatch.dnd.applyMyChoice());
    QCOMPARE(mismatch.transport.commits.size(), 3);
    QCOMPARE(mismatch.transport.commits.constLast().revision, quint64(2));
    QVERIFY(!mismatch.schedule.pending());

    Harness replacement;
    QVERIFY(replacement.establish());
    QVERIFY(replacement.dnd.requestSet(true));
    replacement.reply(SettingsWireStatus::Applied, 1, 2, Dnd, true);
    QVERIFY(replacement.dnd.saving());
    Q_EMIT replacement.transport.ownerChanged(QStringLiteral(":1.72"));
    QVERIFY(replacement.dnd.unavailable());
    QVERIFY(!replacement.dnd.saving());
    QVERIFY(replacement.answer(QStringLiteral(":1.72"),
                               QStringLiteral("notification-epoch-b"), 1,
                               settingsValues()));
    QVERIFY(replacement.dnd.ready());
    QVERIFY(!replacement.dnd.enabled());
    QCOMPARE(replacement.transport.commits.size(), 1);

    Harness readbackLost;
    QVERIFY(readbackLost.establish());
    QVERIFY(readbackLost.dnd.requestSet(true));
    readbackLost.reply(SettingsWireStatus::Applied, 1, 2, Dnd, true);
    QTRY_VERIFY(!readbackLost.transport.snapshots.isEmpty());
    const auto staleRead = readbackLost.transport.snapshots.takeFirst();
    Q_EMIT readbackLost.transport.requestFailed(staleRead.token, staleRead.owner,
        QStringLiteral("org.freedesktop.DBus.Error.NoReply"),
        QStringLiteral("DND readback lost"));
    QVERIFY(readbackLost.dnd.unavailable());
    QVERIFY(!readbackLost.dnd.saving());
    QVERIFY(!readbackLost.dnd.enabled());
    QCOMPARE(readbackLost.transport.commits.size(), 1);
}

QTEST_GUILESS_MAIN(NotificationScheduleOutcomeTests)
#include "tst_notification_schedule_outcomes.moc"
