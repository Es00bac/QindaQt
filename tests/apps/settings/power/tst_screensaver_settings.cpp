// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_power/lock_screen_saver_store.h>
#include <qindaqt/apps/settings_power/screensaver_settings.h>

#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/session/desktop_controls/settings1_screensaver_preferences.h>

#include <QtTest>

#include <memory>

using namespace QindaQt::Apps::SettingsPower;
using namespace QindaQt::Session::DesktopControls;
using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

namespace {

constexpr auto kSaverKey = "power.screensaver";
constexpr auto kMinutesKey = "power.screensaverMinutes";

// AGENT-NOTE: the screensaver route owns a two-key scope, so this fake keeps a
// value map rather than the single value the idle display fake carries. An
// applied commit writes its own operation back into that map, which is how the
// route's "only a confirmed snapshot reconciles the rows" contract is proven.
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

    void requestSnapshot(quint64 token, const QString &owner, const QStringList &) override
    {
        QMetaObject::invokeMethod(
            this,
            [this, token, owner] {
                Q_EMIT snapshotReceived(token, owner, snapshotWire(++m_revision));
            },
            Qt::QueuedConnection);
    }

    struct CommitRequest {
        quint64 token = 0;
        QString owner;
        QString epoch;
        quint64 baseRevision = 0;
    };

    void commit(quint64 token, const QString &owner, const QString &epoch,
                quint64 baseRevision, const QVariantList &operations) override
    {
        for (const QVariant &operation : operations) {
            m_committedOperations.append(operation.toMap());
        }
        m_commits.append(CommitRequest{token, owner, epoch, baseRevision});
    }

    void requestActivation() override {}

    // Answers the pending commit synchronously: the model sees exactly one
    // deterministic reply per write, and an applied one becomes new truth.
    void replyToLastCommit(SettingsWireStatus status)
    {
        QVERIFY(!m_commits.isEmpty());
        QVERIFY(!m_committedOperations.isEmpty());
        const CommitRequest request = m_commits.takeLast();
        const QVariantMap operation = m_committedOperations.constLast();
        m_revision = request.baseRevision + 1;
        const quint64 after =
            status == SettingsWireStatus::Applied ? m_revision : request.baseRevision;
        m_commitStatus = status;
        QStringList changed;
        if (status == SettingsWireStatus::Applied) {
            const QString key = operation.value(QStringLiteral("key")).toString();
            m_values.insert(key, operation.value(QStringLiteral("value")));
            changed.append(key);
        }
        Q_EMIT commitReceived(request.token, request.owner,
                              commitWire(request.baseRevision, after, changed));
    }

    [[nodiscard]] int pendingCommitCount() const noexcept
    {
        return static_cast<int>(m_commits.size());
    }

    void announceOwner()
    {
        QMetaObject::invokeMethod(
            this, [this] { Q_EMIT ownerChanged(QStringLiteral(":1.11")); },
            Qt::QueuedConnection);
    }

    void setValue(const char *key, const QVariant &value)
    {
        m_values.insert(QString::fromLatin1(key), value);
    }

    [[nodiscard]] const QList<QVariantMap> &committedOperations() const noexcept
    {
        return m_committedOperations;
    }

    [[nodiscard]] QVariantMap snapshotWire(quint64 revision) const
    {
        return {{QLatin1StringView(WireContract::FieldStatus),
                 quint32(SettingsWireStatus::Applied)},
                {QLatin1StringView(WireContract::FieldWireSchemaVersion),
                 WireContract::WireSchemaVersion},
                {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
                {QLatin1StringView(WireContract::FieldEpoch), m_epoch},
                {QLatin1StringView(WireContract::FieldRevision), revision},
                {QLatin1StringView(WireContract::FieldValues), m_values},
                {QLatin1StringView(WireContract::FieldSourceLayers), sourceLayers()},
                {QLatin1StringView(WireContract::FieldMessage), QString{}}};
    }

private:
    [[nodiscard]] QVariantMap sourceLayers() const
    {
        QVariantMap layers;
        for (auto it = m_values.constBegin(); it != m_values.constEnd(); ++it) {
            layers.insert(it.key(), QStringLiteral("user-overrides"));
        }
        return layers;
    }

    [[nodiscard]] QVariantMap commitWire(quint64 before, quint64 after,
                                         const QStringList &changed) const
    {
        return {{QLatin1StringView(WireContract::FieldStatus), quint32(m_commitStatus)},
                {QLatin1StringView(WireContract::FieldWireSchemaVersion),
                 WireContract::WireSchemaVersion},
                {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
                {QLatin1StringView(WireContract::FieldEpoch), m_epoch},
                {QLatin1StringView(WireContract::FieldRevisionBefore), before},
                {QLatin1StringView(WireContract::FieldRevisionAfter), after},
                {QLatin1StringView(WireContract::FieldValues), m_values},
                {QLatin1StringView(WireContract::FieldSourceLayers), sourceLayers()},
                {QLatin1StringView(WireContract::FieldChangedKeys), changed},
                {QLatin1StringView(WireContract::FieldMessage), QString{}}};
    }

    QString m_epoch = QStringLiteral("epoch-11");
    quint64 m_revision = 0;
    QVariantMap m_values{{QString::fromLatin1(kSaverKey), QStringLiteral("none")},
                         {QString::fromLatin1(kMinutesKey),
                          QVariant::fromValue<qint64>(5)}};
    SettingsWireStatus m_commitStatus = SettingsWireStatus::Applied;
    QList<QVariantMap> m_committedOperations;
    QList<CommitRequest> m_commits;
};

// The greeter mirror, recorded rather than written: a route test must never
// depend on a real kscreenlockerrc, and the mirror's own file format has its
// own row (qindaqt.settings-lock-screen-saver-store).
class FakeLockScreenSaverStore final : public LockScreenSaverStore {
public:
    [[nodiscard]] bool save(const QString &saverToken, QString *error) override
    {
        ++m_saves;
        if (m_failure.isEmpty()) {
            m_current = saverToken;
            if (error != nullptr) error->clear();
            return true;
        }
        if (error != nullptr) *error = m_failure;
        return false;
    }

    [[nodiscard]] QString currentSaver() const override { return m_current; }

    void failWith(const QString &message) { m_failure = message; }
    [[nodiscard]] int saves() const noexcept { return m_saves; }

private:
    QString m_current = QStringLiteral("none");
    QString m_failure;
    int m_saves = 0;
};

} // namespace

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
    void retryClearsErrorAndNeverWrites();
    void confirmedSaverReachesTheLockScreen();
    void refusedCommitNeverReachesTheLockScreen();
    void lockScreenFailureIsItsOwnError();
    void aSaverWithNoSceneLeavesTheLockWallpaperAlone();

private:
    std::unique_ptr<FakeSettingsTransport> m_transport;
    std::unique_ptr<SettingsClient> m_client;
    std::unique_ptr<Settings1ScreensaverPreferences> m_preferences;
    std::unique_ptr<FakeLockScreenSaverStore> m_lockScreen;
    std::unique_ptr<ScreensaverSettingsModel> m_model;
};

void ScreensaverSettingsModelTest::init() {
    // AGENT-GUARD: destroy the client before the transport it borrows.
    m_client.reset();
    m_transport = std::make_unique<FakeSettingsTransport>();
    m_client = std::make_unique<SettingsClient>(
        *m_transport, Settings1ScreensaverPreferences::scopedKeys());
    m_preferences = std::make_unique<Settings1ScreensaverPreferences>(*m_client);
    m_lockScreen = std::make_unique<FakeLockScreenSaverStore>();
    m_model = std::make_unique<ScreensaverSettingsModel>(*m_preferences, *m_client,
                                                        *m_lockScreen);
    QString error;
    QVERIFY(m_client->start(&error));
}

void ScreensaverSettingsModelTest::initialTruthReflectsThePersistedSnapshot() {
    m_transport->setValue(kSaverKey, QStringLiteral("circuit-reef"));
    m_transport->setValue(kMinutesKey, QVariant::fromValue<qint64>(20));
    m_transport->announceOwner();
    QTRY_COMPARE(m_model->saver(), QStringLiteral("circuit-reef"));
    QCOMPARE(m_model->minutes(), 20);
    QVERIFY(m_model->enabled());
    QVERIFY(m_model->statusText().contains(QStringLiteral("Circuit Reef")));
    QVERIFY(m_model->statusText().contains(QStringLiteral("20")));
    QVERIFY(m_model->errorText().isEmpty());
}

void ScreensaverSettingsModelTest::unknownPersistedTokenReadsAsNoSaver() {
    // AGENT-GUARD: a stale or hand-edited token must never reach QProcess as a
    // program name, so an unrecognized saver reads as "none" on both sides.
    m_transport->setValue(kSaverKey, QStringLiteral("xscreensaver"));
    m_transport->announceOwner();
    QTRY_VERIFY(m_client->snapshot().has_value());
    QCOMPARE(m_model->saver(), ScreensaverPreferences::noneToken());
    QVERIFY(!m_model->enabled());
    QVERIFY(!m_model->statusText().contains(QStringLiteral("xscreensaver")));
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
    QVERIFY(!m_model->enabled());

    QVERIFY(m_model->setSaver(QStringLiteral("qinda-patrol")));
    QVERIFY(m_model->busy());
    QCOMPARE(m_transport->committedOperations().constFirst()
                 .value(QStringLiteral("key"))
                 .toString(),
             QString::fromLatin1(kSaverKey));
    m_transport->replyToLastCommit(SettingsWireStatus::Applied);
    QTRY_VERIFY(m_model->enabled());
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

void ScreensaverSettingsModelTest::retryClearsErrorAndNeverWrites() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_client->snapshot().has_value());
    QVERIFY(m_model->setSaver(QStringLiteral("qinda-patrol")));
    m_transport->replyToLastCommit(SettingsWireStatus::PersistenceFailed);
    QTRY_VERIFY(!m_model->errorText().isEmpty());
    const int writes = static_cast<int>(m_transport->committedOperations().size());

    QVERIFY(m_model->retry());
    QVERIFY(m_model->errorText().isEmpty());
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
    // And the section says so rather than implying the lock screen changed.
    QVERIFY(m_model->statusText().contains(QStringLiteral("Prism Brawl")));
    QVERIFY(m_model->statusText().contains(QStringLiteral("its own wallpaper")));
}

QTEST_MAIN(ScreensaverSettingsModelTest)
#include "tst_screensaver_settings.moc"
