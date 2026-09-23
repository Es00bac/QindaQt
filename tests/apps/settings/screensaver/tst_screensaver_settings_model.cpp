// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_screensaver/lock_screen_saver_store.h>
#include <qindaqt/apps/settings_screensaver/screensaver_preview.h>
#include <qindaqt/apps/settings_screensaver/screensaver_settings_model.h>

#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/session/desktop_controls/screensaver_catalog.h>
#include <qindaqt/session/desktop_controls/settings1_screensaver_preferences.h>

#include <QtTest>

#include "screensaver_model_test_support.h"

#include <memory>

using namespace QindaQt::Apps::SettingsScreensaver;
using namespace QindaQt::Session::DesktopControls;
using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;
using ScreensaverTestSupport::FakeScreensaverCatalog;
using ScreensaverTestSupport::FakeScreensaverPreview;
using ScreensaverTestSupport::FakeLockScreenSaverStore;
using ScreensaverTestSupport::FakeSettingsTransport;
using ScreensaverTestSupport::kSaverKey;
using ScreensaverTestSupport::kMinutesKey;

class ScreensaverSettingsModelTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void init();
    void initialTruthReflectsThePersistedSnapshot();
    void unknownPersistedTokenReadsAsNoSaver();
    void unknownSaverIsRejectedBeforeWriting();
    void minutesAreBoundsCheckedBeforeWriting();
    void appliedSaverCommitReconcilesStatus();
    void rejectedCommitReportsTheFailure();
    void busyModelRejectsFurtherWrites();
    void retryNeverReplaysOrErasesRefusal();
    void confirmedSaverReachesTheLockScreen();
    void refusedCommitNeverReachesTheLockScreen();
    void lockScreenFailureIsItsOwnError();
    void aSaverWithNoSceneLeavesTheLockWallpaperAlone();
    void blankIsItsOwnChoice();
    void saverOptionsListTheBuiltInsThenTheDiscovered();
    void previewStartsWithPersistedTruth();
    void defaultValuedFirstSnapshotEstablishesAuthority();
    void occupiedReadLaneRefusesWritesAndRecovers();
    void appliedWaitsForSameLineageReadback();
    void unchangedRefreshRetainsRefusal();
    void lostReplyAndOwnerReplacementNeverReplay();
    void appliedMismatchIsConflict();
    void conflictAndExternalUpdateRetainDiagnostic();
    void ownerReplacementDuringAppliedReadbackCannotConfirm();

private:
    std::unique_ptr<FakeSettingsTransport> m_transport;
    std::unique_ptr<SettingsClient> m_client;
    FakeScreensaverCatalog m_catalog;
    std::unique_ptr<Settings1ScreensaverPreferences> m_preferences;
    std::unique_ptr<FakeLockScreenSaverStore> m_lockScreen;
    std::unique_ptr<FakeScreensaverPreview> m_preview;
    std::unique_ptr<ScreensaverSettingsModel> m_model;
};

void ScreensaverSettingsModelTest::init() {
    // AGENT-GUARD: destroy the client before the transport it borrows.
    m_client.reset();
    m_transport = std::make_unique<FakeSettingsTransport>();
    m_client = std::make_unique<SettingsClient>(
        *m_transport, Settings1ScreensaverPreferences::scopedKeys());
    m_preferences =
        std::make_unique<Settings1ScreensaverPreferences>(*m_client, m_catalog);
    m_lockScreen = std::make_unique<FakeLockScreenSaverStore>();
    m_preview = std::make_unique<FakeScreensaverPreview>();
    m_model = std::make_unique<ScreensaverSettingsModel>(
        *m_preferences, *m_client, m_catalog, *m_lockScreen, *m_preview);
    QString error;
    QVERIFY(m_client->start(&error));
}

void ScreensaverSettingsModelTest::initialTruthReflectsThePersistedSnapshot() {
    m_transport->setValue(kSaverKey, "circuit-reef");
    m_transport->setValue(kMinutesKey, QVariant::fromValue<qint64>(20));
    m_transport->announceOwner();
    QTRY_COMPARE(m_model->saver(), QStringLiteral("circuit-reef"));
    QCOMPARE(m_model->minutes(), 20);
    QVERIFY(m_model->delayEnabled());
    QVERIFY(m_model->statusText().contains(QStringLiteral("Circuit Reef")));
    QVERIFY(m_model->statusText().contains(QStringLiteral("20")));
    QVERIFY(m_model->errorText().isEmpty());
}

void ScreensaverSettingsModelTest::unknownPersistedTokenReadsAsNoSaver() {
    // AGENT-GUARD: a stale or hand-edited token must never reach QProcess as a
    // program name, so an unrecognized saver reads as "none" on both sides.
    m_transport->setValue(kSaverKey, "xscreensaver");
    m_transport->announceOwner();
    QTRY_VERIFY(m_client->snapshot().has_value());
    QCOMPARE(m_model->saver(), ScreensaverPreferences::noneToken());
    QVERIFY(!m_model->delayEnabled());
    QVERIFY(!m_model->statusText().contains(QStringLiteral("xscreensaver")));
    // The UI-normalized None is not yet the raw persisted value. Selecting
    // None must repair the unknown token rather than claiming a no-op.
    QVERIFY(m_model->setSaver(ScreensaverPreferences::noneToken()));
    QCOMPARE(m_transport->committedOperations().size(), 1);
    m_transport->replyToLastCommit(SettingsWireStatus::Applied);
    QTRY_VERIFY(!m_model->busy());
}

void ScreensaverSettingsModelTest::unknownSaverIsRejectedBeforeWriting() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_client->snapshot().has_value());
    QVERIFY(!m_model->setSaver(QStringLiteral("xscreensaver")));
    QVERIFY(!m_model->errorText().isEmpty());
    QCOMPARE(m_transport->committedOperations().size(), 0);
    QVERIFY(!m_model->busy());
}

void ScreensaverSettingsModelTest::minutesAreBoundsCheckedBeforeWriting() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_client->snapshot().has_value());
    QVERIFY(!m_model->setMinutes(0));
    QVERIFY(!m_model->setMinutes(ScreensaverPreferences::maximumTimeoutMinutes() + 1));
    QVERIFY(!m_model->errorText().isEmpty());
    QCOMPARE(m_transport->committedOperations().size(), 0);

    QVERIFY(m_model->setMinutes(ScreensaverPreferences::maximumTimeoutMinutes()));
    QCOMPARE(m_transport->committedOperations().size(), 1);
    const QVariantMap operation = m_transport->committedOperations().constFirst();
    QCOMPARE(operation.value(QStringLiteral("key")).toString(),
             QString::fromLatin1(kMinutesKey));
    QCOMPARE(operation.value(QStringLiteral("value")).toLongLong(),
             ScreensaverPreferences::maximumTimeoutMinutes());
    m_transport->replyToLastCommit(SettingsWireStatus::Applied);
    QTRY_COMPARE(m_model->minutes(), ScreensaverPreferences::maximumTimeoutMinutes());
    QVERIFY(!m_model->busy());
}

void ScreensaverSettingsModelTest::appliedSaverCommitReconcilesStatus() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_client->snapshot().has_value());
    QVERIFY(!m_model->delayEnabled());

    QVERIFY(m_model->setSaver(QStringLiteral("qinda-patrol")));
    QVERIFY(m_model->busy());
    QCOMPARE(m_transport->committedOperations().constFirst()
                 .value(QStringLiteral("key"))
                 .toString(),
             QString::fromLatin1(kSaverKey));
    m_transport->replyToLastCommit(SettingsWireStatus::Applied);
    QTRY_VERIFY(m_model->delayEnabled());
    QVERIFY(!m_model->busy());
    QVERIFY2(m_model->errorText().isEmpty(), qPrintable(m_model->errorText()));
    QVERIFY(m_model->statusText().contains(QStringLiteral("Qinda Patrol")));
}

void ScreensaverSettingsModelTest::rejectedCommitReportsTheFailure() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_client->snapshot().has_value());
    QVERIFY(m_model->setSaver(QStringLiteral("qinda-patrol")));
    m_transport->replyToLastCommit(SettingsWireStatus::PersistenceFailed);
    QVERIFY(!m_model->busy());
    QVERIFY(!m_model->errorText().isEmpty());
    // A refused write leaves the route on persisted truth, never on the
    // selection the user tried.
    QCOMPARE(m_model->saver(), ScreensaverPreferences::noneToken());
}

void ScreensaverSettingsModelTest::busyModelRejectsFurtherWrites() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_client->snapshot().has_value());
    QVERIFY(m_model->setSaver(QStringLiteral("qinda-patrol")));
    QVERIFY(m_model->busy());
    QVERIFY(!m_model->setMinutes(30));
    QVERIFY(!m_model->setSaver(QStringLiteral("circuit-reef")));
    QCOMPARE(m_transport->committedOperations().size(), 1);
    QCOMPARE(m_transport->pendingCommitCount(), 1);
}

void ScreensaverSettingsModelTest::retryNeverReplaysOrErasesRefusal() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_client->snapshot().has_value());
    QVERIFY(m_model->setSaver(QStringLiteral("qinda-patrol")));
    m_transport->replyToLastCommit(SettingsWireStatus::PersistenceFailed);
    QTRY_VERIFY(!m_model->errorText().isEmpty());
    const int writes = static_cast<int>(m_transport->committedOperations().size());

    QVERIFY(m_model->retry());
    QTRY_VERIFY(m_model->canEdit());
    QVERIFY(!m_model->errorText().isEmpty());
    // Retry only re-reads: it must never replay the write that failed.
    QCOMPARE(static_cast<int>(m_transport->committedOperations().size()), writes);
}

void ScreensaverSettingsModelTest::confirmedSaverReachesTheLockScreen() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_client->snapshot().has_value());
    QCOMPARE(m_lockScreen->currentSaver(), ScreensaverPreferences::noneToken());

    QVERIFY(m_model->setSaver(QStringLiteral("circuit-reef")));
    // Still nothing: a write in flight is not truth.
    QCOMPARE(m_lockScreen->currentSaver(), ScreensaverPreferences::noneToken());

    m_transport->replyToLastCommit(SettingsWireStatus::Applied);
    QTRY_COMPARE(m_lockScreen->currentSaver(), QStringLiteral("circuit-reef"));
    QVERIFY2(m_model->errorText().isEmpty(), qPrintable(m_model->errorText()));
}

void ScreensaverSettingsModelTest::refusedCommitNeverReachesTheLockScreen() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_client->snapshot().has_value());
    QVERIFY(m_model->setSaver(QStringLiteral("qinda-patrol")));
    m_transport->replyToLastCommit(SettingsWireStatus::PersistenceFailed);
    QTRY_VERIFY(!m_model->errorText().isEmpty());
    // AGENT-GUARD: a locked screen must show what is actually persisted. A
    // refused commit that still reached the greeter would leave the lock
    // screen showing a saver the unlocked session does not have.
    QCOMPARE(m_lockScreen->currentSaver(), ScreensaverPreferences::noneToken());
    QCOMPARE(m_lockScreen->saves(), 0);
}

void ScreensaverSettingsModelTest::lockScreenFailureIsItsOwnError() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_client->snapshot().has_value());
    m_lockScreen->failWith(QStringLiteral("read-only configuration"));

    QVERIFY(m_model->setSaver(QStringLiteral("qinda-patrol")));
    m_transport->replyToLastCommit(SettingsWireStatus::Applied);
    QTRY_COMPARE(m_model->saver(), QStringLiteral("qinda-patrol"));
    // The preference is persisted; only the mirror failed, and the message
    // must say so rather than claim the choice was lost.
    QVERIFY(m_model->errorText().contains(QStringLiteral("lock screen")));
    QVERIFY(m_model->statusText().contains(QStringLiteral("Qinda Patrol")));
}

void ScreensaverSettingsModelTest::aSaverWithNoSceneLeavesTheLockWallpaperAlone() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_client->snapshot().has_value());

    // A saver the greeter can draw takes the lock wallpaper over.
    QVERIFY(m_model->setSaver(QStringLiteral("circuit-reef")));
    m_transport->replyToLastCommit(SettingsWireStatus::Applied);
    QTRY_COMPARE(m_lockScreen->currentSaver(), QStringLiteral("circuit-reef"));
    QVERIFY(m_model->statusText().contains(QStringLiteral("locked")));

    // AGENT-GUARD: one that ships no QML module must hand it back instead of
    // pointing the greeter at a wallpaper plugin with nothing to draw.
    QVERIFY(m_model->setSaver(QStringLiteral("prism-brawl")));
    m_transport->replyToLastCommit(SettingsWireStatus::Applied);
    QTRY_COMPARE(m_model->saver(), QStringLiteral("prism-brawl"));
    QCOMPARE(m_lockScreen->currentSaver(), ScreensaverPreferences::noneToken());
    // And the page says so rather than implying the lock screen changed.
    QVERIFY(m_model->statusText().contains(QStringLiteral("Prism Brawl")));
    QVERIFY(m_model->statusText().contains(QStringLiteral("its own wallpaper")));
}

void ScreensaverSettingsModelTest::blankIsItsOwnChoice() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_client->snapshot().has_value());

    QVERIFY(m_model->setSaver(ScreensaverPreferences::blankToken()));
    m_transport->replyToLastCommit(SettingsWireStatus::Applied);
    QTRY_COMPARE(m_model->saver(), ScreensaverPreferences::blankToken());
    // "blank" arms no program, so the delay row is disabled...
    QVERIFY(!m_model->delayEnabled());
    // ...but the lock screen still mirrors it: the plugin's painted ground IS
    // the blank screen the user asked for.
    QTRY_COMPARE(m_lockScreen->currentSaver(), ScreensaverPreferences::blankToken());
    QVERIFY(m_model->statusText().contains(QStringLiteral("dark screen")));
}

void ScreensaverSettingsModelTest::saverOptionsListTheBuiltInsThenTheDiscovered() {
    const QVariantList options = m_model->saverOptions();
    QCOMPARE(options.size(), 5);
    QCOMPARE(options.at(0).toMap().value(QStringLiteral("token")).toString(),
             ScreensaverPreferences::noneToken());
    QCOMPARE(options.at(1).toMap().value(QStringLiteral("token")).toString(),
             ScreensaverPreferences::blankToken());
    // Discovered savers sort by name, case-insensitively: Circuit Reef,
    // Prism Brawl, Qinda Patrol.
    QCOMPARE(options.at(2).toMap().value(QStringLiteral("token")).toString(),
             QStringLiteral("circuit-reef"));
    QCOMPARE(options.at(3).toMap().value(QStringLiteral("token")).toString(),
             QStringLiteral("prism-brawl"));
    QCOMPARE(options.at(4).toMap().value(QStringLiteral("token")).toString(),
             QStringLiteral("qinda-patrol"));
    QCOMPARE(options.at(3).toMap().value(QStringLiteral("showsOnLockScreen")).toBool(),
             false);
    QCOMPARE(options.at(4).toMap().value(QStringLiteral("showsOnLockScreen")).toBool(),
             true);
}

void ScreensaverSettingsModelTest::previewStartsWithPersistedTruth() {
    m_transport->setValue(kSaverKey, "circuit-reef");
    m_transport->announceOwner();
    QTRY_COMPARE(m_model->saver(), QStringLiteral("circuit-reef"));
    QVERIFY(m_model->previewAvailable());

    QVERIFY(m_model->preview());
    QCOMPARE(m_preview->starts, 1);
    // The preview always shows persisted truth, not a write in flight.
    QCOMPARE(m_preview->lastToken, QStringLiteral("circuit-reef"));
    QVERIFY(m_model->previewSummary().contains(QStringLiteral("never locked")));

    // "none" previews nothing.
    QVERIFY(m_model->setSaver(ScreensaverPreferences::noneToken()));
    m_transport->replyToLastCommit(SettingsWireStatus::Applied);
    QTRY_COMPARE(m_model->saver(), ScreensaverPreferences::noneToken());
    QVERIFY(!m_model->previewAvailable());
    QVERIFY(!m_model->preview());
    QCOMPARE(m_preview->starts, 1);
}

void ScreensaverSettingsModelTest::defaultValuedFirstSnapshotEstablishesAuthority() {
    QCOMPARE(m_model->saver(), QString{});
    QCOMPARE(m_model->minutes(), 0);
    QVERIFY(!m_model->hasConfirmed());
    QVERIFY(!m_model->available());
    QVERIFY(!m_model->canEdit());
    QVERIFY(!m_model->statusText().contains(QStringLiteral("No screensaver starts")));
    m_transport->announceOwner();
    QTRY_VERIFY(m_model->hasConfirmed());
    QCOMPARE(m_model->saver(), ScreensaverPreferences::noneToken());
    QCOMPARE(m_model->minutes(), 5);
    QVERIFY(m_model->available());
    QVERIFY(m_model->canEdit());
}

void ScreensaverSettingsModelTest::occupiedReadLaneRefusesWritesAndRecovers() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_model->canEdit());
    m_transport->autoSnapshots = false;
    m_client->refresh();
    QTRY_COMPARE(m_transport->pendingSnapshotCount(), 1);
    QVERIFY(!m_model->canEdit());
    QVERIFY(!m_model->setSaver(QStringLiteral("qinda-patrol")));
    QCOMPARE(m_transport->committedOperations().size(), 0);
    QVERIFY(!m_model->errorText().isEmpty());
    m_transport->replyToLastSnapshot();
    QTRY_VERIFY(m_model->canEdit());
    QVERIFY(!m_model->errorText().isEmpty());
}

void ScreensaverSettingsModelTest::appliedWaitsForSameLineageReadback() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_model->canEdit());
    m_transport->autoSnapshots = false;
    QVERIFY(m_model->setSaver(QStringLiteral("qinda-patrol")));
    m_transport->replyToLastCommit(SettingsWireStatus::Applied);
    QTRY_COMPARE(m_transport->pendingSnapshotCount(), 1);
    QVERIFY(m_model->busy());
    QCOMPARE(m_model->saver(), ScreensaverPreferences::noneToken());
    // A stale read at the previous revision cannot confirm the Applied reply.
    m_transport->setValue(kSaverKey, QStringLiteral("none"));
    m_transport->replyToLastSnapshot(m_client->snapshot()->revision);
    QVERIFY(m_model->busy());
    QVERIFY(!m_model->canEdit());
    m_client->refresh();
    QTRY_COMPARE(m_transport->pendingSnapshotCount(), 1);
    m_transport->setValue(kSaverKey, QStringLiteral("qinda-patrol"));
    m_transport->replyToLastSnapshot(m_client->snapshot()->revision + 1);
    QTRY_VERIFY(!m_model->busy());
    QCOMPARE(m_model->saver(), QStringLiteral("qinda-patrol"));
    QVERIFY(m_model->canEdit());
    QVERIFY(m_model->errorText().isEmpty());
}

void ScreensaverSettingsModelTest::unchangedRefreshRetainsRefusal() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_model->canEdit());
    QVERIFY(m_model->setSaver(QStringLiteral("qinda-patrol")));
    m_transport->replyToLastCommit(SettingsWireStatus::PersistenceFailed);
    QTRY_VERIFY(m_model->available());
    QVERIFY(!m_model->errorText().isEmpty());
    const QString refused = m_model->errorText();
    m_client->refresh();
    QTRY_VERIFY(m_model->canEdit());
    QCOMPARE(m_model->errorText(), refused);
    QCOMPARE(m_model->saver(), ScreensaverPreferences::noneToken());
}

void ScreensaverSettingsModelTest::lostReplyAndOwnerReplacementNeverReplay() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_model->canEdit());
    QVERIFY(m_model->setSaver(QStringLiteral("qinda-patrol")));
    m_transport->failLastCommit();
    QTRY_VERIFY(!m_model->busy());
    QVERIFY(m_model->uncertain());
    QVERIFY(!m_model->errorText().isEmpty());
    const qsizetype writes = m_transport->committedOperations().size();
    QVERIFY(m_model->retry());
    QTRY_VERIFY(m_model->canEdit());
    QVERIFY(m_model->uncertain());
    QVERIFY(!m_model->errorText().isEmpty());
    QCOMPARE(m_transport->committedOperations().size(), writes);
    QVERIFY(m_model->setSaver(QStringLiteral("circuit-reef")));
    m_transport->announceOwner(QStringLiteral(":1.12"));
    QTRY_VERIFY(!m_model->busy());
    QVERIFY(m_model->uncertain());
    QTRY_VERIFY(m_model->available());
    QCOMPARE(m_model->saver(), ScreensaverPreferences::noneToken());
    QCOMPARE(m_transport->committedOperations().size(), writes + 1);
}

void ScreensaverSettingsModelTest::appliedMismatchIsConflict() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_model->canEdit());
    m_transport->autoSnapshots = false;
    QVERIFY(m_model->setSaver(QStringLiteral("qinda-patrol")));
    m_transport->replyToLastCommit(SettingsWireStatus::Applied);
    QTRY_COMPARE(m_transport->pendingSnapshotCount(), 1);
    m_transport->setValue(kSaverKey, QStringLiteral("circuit-reef"));
    m_transport->replyToLastSnapshot();
    QTRY_VERIFY(!m_model->busy());
    QVERIFY(m_model->conflict());
    QCOMPARE(m_model->saver(), QStringLiteral("circuit-reef"));
    QVERIFY(m_model->errorText().contains(QStringLiteral("differs")));
    QCOMPARE(m_lockScreen->currentSaver(), QStringLiteral("circuit-reef"));
}

void ScreensaverSettingsModelTest::conflictAndExternalUpdateRetainDiagnostic() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_model->canEdit());
    QVERIFY(m_model->setSaver(QStringLiteral("qinda-patrol")));
    m_transport->replyToLastCommit(SettingsWireStatus::Conflict);
    QTRY_VERIFY(m_model->canEdit());
    QVERIFY(m_model->conflict());
    QVERIFY(!m_model->errorText().isEmpty());
    const QString failure = m_model->errorText();
    m_transport->setValue(kSaverKey, QStringLiteral("circuit-reef"));
    m_client->refresh();
    QTRY_COMPARE(m_model->saver(), QStringLiteral("circuit-reef"));
    QCOMPARE(m_model->errorText(), failure);
    QVERIFY(m_model->conflict());
}

void ScreensaverSettingsModelTest::ownerReplacementDuringAppliedReadbackCannotConfirm() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_model->canEdit());
    m_transport->autoSnapshots = false;
    QVERIFY(m_model->setSaver(QStringLiteral("qinda-patrol")));
    m_transport->replyToLastCommit(SettingsWireStatus::Applied);
    QTRY_COMPARE(m_transport->pendingSnapshotCount(), 1);
    QVERIFY(m_model->busy());
    m_transport->announceOwner(QStringLiteral(":1.12"));
    QTRY_VERIFY(!m_model->busy());
    QVERIFY(m_model->uncertain());
    QTRY_COMPARE(m_transport->pendingSnapshotCount(), 2);
    m_transport->replyToLastSnapshot();
    QTRY_VERIFY(m_model->available());
    QCOMPARE(m_model->saver(), QStringLiteral("qinda-patrol"));
    QVERIFY(m_model->uncertain());
    QVERIFY(!m_model->errorText().isEmpty());
    // The old owner's queued reply is ignored even though its value matches.
    m_transport->replyToLastSnapshot();
    QVERIFY(m_model->uncertain());
    QCOMPARE(m_transport->committedOperations().size(), 1);
}

QTEST_MAIN(ScreensaverSettingsModelTest)
#include "tst_screensaver_settings_model.moc"
