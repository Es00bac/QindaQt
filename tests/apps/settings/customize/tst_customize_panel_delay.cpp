// SPDX-License-Identifier: GPL-3.0-or-later

#include "customize_test_support.h"

#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QtTest>

using namespace QindaQt::Apps::SettingsCustomize;
using namespace QindaQt::Apps::SettingsCustomize::TestSupport;
using namespace QindaQt::Services::SettingsProtocol;

namespace {

QVariantMap delayCommitWire(SettingsWireStatus status, qint64 value,
                            quint64 before = 7, quint64 after = 8,
                            QString message = {})
{
    QVariantMap wire = commitWire(status, QStringLiteral("fixture"), message);
    wire.insert(QLatin1StringView(WireContract::FieldRevisionBefore), before);
    wire.insert(QLatin1StringView(WireContract::FieldRevisionAfter), after);
    wire.insert(QLatin1StringView(WireContract::FieldValues),
                QVariantMap{{QString(PanelHideDelaySettingsKey), value}});
    wire.insert(QLatin1StringView(WireContract::FieldSourceLayers),
                QVariantMap{{QString(PanelHideDelaySettingsKey),
                             QStringLiteral("user-overrides")}});
    wire.insert(QLatin1StringView(WireContract::FieldChangedKeys),
                status == SettingsWireStatus::Applied && after > before
                    ? QStringList{QString(PanelHideDelaySettingsKey)}
                    : QStringList{});
    return wire;
}

QVariantMap profileCommitWire(const QString &profileId, quint64 before = 8,
                              quint64 after = 9)
{
    QVariantMap wire = commitWire(SettingsWireStatus::Applied, profileId);
    wire.insert(QLatin1StringView(WireContract::FieldRevisionBefore), before);
    wire.insert(QLatin1StringView(WireContract::FieldRevisionAfter), after);
    wire.insert(QLatin1StringView(WireContract::FieldChangedKeys),
                QStringList{QString(LayoutProfileSettingsKey)});
    return wire;
}

void replySnapshot(ModelHarness &harness, const QString &profileId,
                   qint64 delay, quint64 revision,
                   const QString &epoch = QStringLiteral("epoch-a"))
{
    Q_ASSERT(!harness.transport.snapshots.isEmpty());
    const auto request = harness.transport.snapshots.takeFirst();
    Q_EMIT harness.transport.snapshotReceived(
        request.token, request.owner,
        panelDelaySnapshotWire(profileId, delay, epoch, revision));
}

} // namespace

class CustomizePanelDelayTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void requiresConfirmedDelayAndAdmission();
    void appliedStaleThenConfirmedPersistsAcrossProfileSwitch();
    void refusalConflictAndUnchangedRefreshStayVisible();
    void ownerReplacementAndLostReplyNeverReplay();
    void staleReadbackExpiresWithoutWriteReplay();
};

void CustomizePanelDelayTests::requiresConfirmedDelayAndAdmission()
{
    ModelHarness h(true);
    QVERIFY(!h.model.panelHideDelayAvailable());
    QVERIFY(!h.model.panelHideDelayEditable());
    QVERIFY(!h.model.setPanelHideDelayMs(500));
    QCOMPARE(h.transport.commits.size(), 0);
    QVERIFY(h.establish());
    QCOMPARE(h.model.panelHideDelayMs(), 250);
    QVERIFY(h.model.panelHideDelayAvailable());
    QVERIFY(h.model.panelHideDelayEditable());
    QVERIFY(!h.model.setPanelHideDelayMs(-1));
    QVERIFY(!h.model.setPanelHideDelayMs(5001));
    QCOMPARE(h.transport.commits.size(), 0);

    h.model.retry();
    QTRY_VERIFY(!h.transport.snapshots.isEmpty());
    QVERIFY(!h.model.panelHideDelayEditable());
    QVERIFY(!h.model.setPanelHideDelayMs(500));
    QCOMPARE(h.transport.commits.size(), 0);
    replySnapshot(h, QStringLiteral("fixture"), 250, 7);
    QTRY_VERIFY(h.model.panelHideDelayEditable());
}

void CustomizePanelDelayTests::appliedStaleThenConfirmedPersistsAcrossProfileSwitch()
{
    ModelHarness h(true);
    QVERIFY(h.establish());
    QVERIFY(h.model.setPanelHideDelayMs(600));
    QCOMPARE(h.transport.commits.size(), 1);
    QVERIFY(h.model.panelHideDelayPending());
    QCOMPARE(h.model.panelHideDelayMs(), 250);
    QVERIFY(!h.model.panelHideDelayEditable());
    // One Settings1 write at a time: no preset switch while the delay is pending.
    QVERIFY(!h.model.canSwitch());
    const auto delayCommit = h.transport.commits.takeFirst();
    QCOMPARE(delayCommit.operations.size(), 1);
    QCOMPARE(delayCommit.operations.first().toMap()
                 .value(QStringLiteral("key")).toString(),
             QString(PanelHideDelaySettingsKey));
    Q_EMIT h.transport.commitReceived(
        delayCommit.token, delayCommit.owner,
        delayCommitWire(SettingsWireStatus::Applied, 600));
    QTRY_VERIFY(!h.transport.snapshots.isEmpty());
    replySnapshot(h, QStringLiteral("fixture"), 250, 7);
    QVERIFY(h.model.panelHideDelayPending());
    QCOMPARE(h.model.panelHideDelayMs(), 250);
    QTRY_VERIFY_WITH_TIMEOUT(!h.transport.snapshots.isEmpty(), 2'000);
    replySnapshot(h, QStringLiteral("fixture"), 600, 8);
    QTRY_VERIFY(!h.model.panelHideDelayPending());
    QCOMPARE(h.model.panelHideDelayMs(), 600);
    QVERIFY(h.model.panelHideDelayEditable());
    QVERIFY(h.model.panelHideDelayStatus().contains(QStringLiteral("every layout")));

    QVERIFY(h.model.activatePreset(QStringLiteral("alternate")));
    QVERIFY(!h.model.panelHideDelayEditable());
    QCOMPARE(h.model.panelHideDelayMs(), 600);
    QTRY_COMPARE(h.transport.commits.size(), 1);
    const auto profileCommit = h.transport.commits.takeFirst();
    Q_EMIT h.transport.commitReceived(
        profileCommit.token, profileCommit.owner,
        profileCommitWire(QStringLiteral("alternate")));
    QTRY_VERIFY(!h.transport.snapshots.isEmpty());
    replySnapshot(h, QStringLiteral("alternate"), 600, 9);
    QTRY_VERIFY(!h.model.busy());
    QCOMPARE(h.model.activePresetId(), QStringLiteral("alternate"));
    QCOMPARE(h.model.panelHideDelayMs(), 600);

    ModelHarness reopened(true);
    QVERIFY(reopened.establish(QStringLiteral("alternate"), 600));
    QCOMPARE(reopened.model.panelHideDelayMs(), 600);
    QVERIFY(reopened.model.panelHideDelayAvailable());
}

void CustomizePanelDelayTests::refusalConflictAndUnchangedRefreshStayVisible()
{
    ModelHarness h(true);
    QVERIFY(h.establish());
    QVERIFY(h.model.setPanelHideDelayMs(500));
    auto commit = h.transport.commits.takeFirst();
    Q_EMIT h.transport.commitReceived(
        commit.token, commit.owner,
        delayCommitWire(SettingsWireStatus::ValidationFailed, 250, 7, 7,
                        QStringLiteral("delay denied")));
    QTRY_VERIFY(!h.model.panelHideDelayPending());
    QVERIFY(h.model.panelHideDelayStatus().contains(QStringLiteral("denied")));
    QTRY_VERIFY(!h.transport.snapshots.isEmpty());
    replySnapshot(h, QStringLiteral("fixture"), 250, 7);
    QVERIFY(h.model.panelHideDelayStatus().contains(QStringLiteral("denied")));
    QCOMPARE(h.model.panelHideDelayMs(), 250);

    QTRY_VERIFY(h.model.panelHideDelayEditable());
    QVERIFY(h.model.setPanelHideDelayMs(500));
    commit = h.transport.commits.takeFirst();
    Q_EMIT h.transport.commitReceived(
        commit.token, commit.owner,
        delayCommitWire(SettingsWireStatus::Applied, 500));
    QTRY_VERIFY(!h.transport.snapshots.isEmpty());
    replySnapshot(h, QStringLiteral("fixture"), 450, 8);
    QTRY_VERIFY(!h.model.panelHideDelayPending());
    QCOMPARE(h.model.panelHideDelayMs(), 450);
    QVERIFY(h.model.panelHideDelayStatus().contains(QStringLiteral("differs")));
    QCOMPARE(h.transport.commits.size(), 0);
}

void CustomizePanelDelayTests::ownerReplacementAndLostReplyNeverReplay()
{
    {
        ModelHarness h(true);
        QVERIFY(h.establish());
        QVERIFY(h.model.setPanelHideDelayMs(800));
        QCOMPARE(h.transport.commits.size(), 1);
        Q_EMIT h.transport.ownerChanged(QStringLiteral(":1.91"));
        QTRY_VERIFY(!h.model.panelHideDelayPending());
        QVERIFY(!h.model.panelHideDelayAvailable());
        QVERIFY(h.model.panelHideDelayStatus().contains(QStringLiteral("changed")));
        QCOMPARE(h.transport.commits.size(), 1);
        QTRY_VERIFY(!h.transport.snapshots.isEmpty());
        replySnapshot(h, QStringLiteral("fixture"), 400, 1,
                      QStringLiteral("epoch-b"));
        QTRY_VERIFY(h.model.panelHideDelayAvailable());
        QCOMPARE(h.model.panelHideDelayMs(), 400);
        QCOMPARE(h.transport.commits.size(), 1);
    }
    {
        ModelHarness h(true);
        QVERIFY(h.establish());
        QVERIFY(h.model.setPanelHideDelayMs(800));
        QTRY_VERIFY_WITH_TIMEOUT(!h.model.panelHideDelayPending(), 1'000);
        QVERIFY(h.model.panelHideDelayStatus().contains(QStringLiteral("uncertain")));
        QCOMPARE(h.transport.commits.size(), 1);
    }
}

void CustomizePanelDelayTests::staleReadbackExpiresWithoutWriteReplay()
{
    ModelHarness h(true);
    QVERIFY(h.establish());
    QVERIFY(h.model.setPanelHideDelayMs(900));
    QCOMPARE(h.transport.commits.size(), 1);
    const auto commit = h.transport.commits.takeFirst();
    Q_EMIT h.transport.commitReceived(
        commit.token, commit.owner,
        delayCommitWire(SettingsWireStatus::Applied, 900));

    QTimer staleReplies;
    staleReplies.setInterval(10);
    connect(&staleReplies, &QTimer::timeout, &h.model, [&] {
        while (!h.transport.snapshots.isEmpty()) {
            replySnapshot(h, QStringLiteral("fixture"), 250, 7);
        }
    });
    staleReplies.start();
    QTRY_VERIFY_WITH_TIMEOUT(!h.model.panelHideDelayPending(), 5'000);
    staleReplies.stop();
    QVERIFY(h.model.panelHideDelayStatus().contains(
        QStringLiteral("could not be confirmed")));
    QCOMPARE(h.model.panelHideDelayMs(), 250);
    QCOMPARE(h.transport.commits.size(), 0);
}

QTEST_GUILESS_MAIN(CustomizePanelDelayTests)
#include "tst_customize_panel_delay.moc"
