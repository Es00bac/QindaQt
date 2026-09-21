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

#include <memory>

using namespace QindaQt::Apps::SettingsScreensaver;
using namespace QindaQt::Session::DesktopControls;
using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

namespace {

constexpr auto kSaverKey = "power.screensaver";
constexpr auto kMinutesKey = "power.screensaverMinutes";

// The installed saver set, fixed for the test: two savers the locker's
// wallpaper plugin can draw and one it cannot, which is the split the
// mirror and the status line must be honest about.
class FakeScreensaverCatalog final : public ScreensaverCatalog {
public:
    [[nodiscard]] QList<ScreensaverCatalogEntry> entries() const override
    {
        return {
            {QStringLiteral("qinda-patrol"), QStringLiteral("Qinda Patrol"),
             QStringLiteral("A patrol on every output"), QStringLiteral("qinda-patrol"),
             {QStringLiteral("--screensaver"), QStringLiteral("--no-metrics")}, true},
            {QStringLiteral("circuit-reef"), QStringLiteral("Circuit Reef"),
             QStringLiteral("A reef circuit"), QStringLiteral("circuit-reef"),
             {QStringLiteral("--screensaver"), QStringLiteral("--private")}, true},
            {QStringLiteral("prism-brawl"), QStringLiteral("Prism Brawl"),
             QStringLiteral("A brawl of prisms"), QStringLiteral("prism-brawl"),
             {QStringLiteral("--screensaver"), QStringLiteral("--mute")}, false},
        };
    }
};

// The preview, recorded rather than launched: the process boundary has its own
// row (qindaqt.settings-screensaver-preview).
class FakeScreensaverPreview final : public ScreensaverPreview {
public:
    using ScreensaverPreview::ScreensaverPreview;

    [[nodiscard]] Kind kindFor(const QString &token) const override
    {
        if (unavailable) return Kind::Unavailable;
        if (token == ScreensaverPreferences::blankToken()) return Kind::TestingGreeter;
        if (token == QStringLiteral("qinda-patrol")
            || token == QStringLiteral("circuit-reef")) {
            return Kind::TestingGreeter;
        }
        if (token == QStringLiteral("prism-brawl")) return Kind::SaverProgram;
        return Kind::Unavailable;
    }

    [[nodiscard]] bool start(const QString &token, QString *error) override
    {
        // Same refusal as the process boundary: an unavailable kind has
        // nothing to launch.
        if (kindFor(token) == Kind::Unavailable) {
            if (error != nullptr) *error = QStringLiteral("nothing to preview");
            return false;
        }
        if (!startOk) {
            if (error != nullptr) *error = QStringLiteral("preview refused");
            return false;
        }
        ++starts;
        lastToken = token;
        return true;
    }

    [[nodiscard]] bool running() const override { return false; }

    bool unavailable = false;
    bool startOk = true;
    int starts = 0;
    QString lastToken;
};

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
// own row (qindaqt.settings-screensaver-lock-screen-saver-store).
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
    void blankIsItsOwnChoice();
    void saverOptionsListTheBuiltInsThenTheDiscovered();
    void previewStartsWithPersistedTruth();

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

QTEST_MAIN(ScreensaverSettingsModelTest)
#include "tst_screensaver_settings_model.moc"
