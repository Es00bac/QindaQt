// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_settings_model.h"
#include "qindaqt/apps/settings_appearance/appearance_values.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/themes/theme_loader.h"

#include <QtTest>

using namespace QindaQt::Apps::SettingsAppearance;
using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

namespace {

class SequenceTransport final : public SettingsTransport {
    Q_OBJECT
public:
    bool start(QString *) override { return true; }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner, const QStringList &) override
    {
        snapshots.append({token, owner});
    }
    void commit(quint64 token, const QString &owner, const QString &epoch,
                quint64 revision, const QVariantList &operations) override
    {
        commits.append({token, owner, epoch, revision, operations});
    }
    void requestActivation() override {}

    struct SnapshotRequest { quint64 token; QString owner; };
    struct CommitRequest {
        quint64 token;
        QString owner;
        QString epoch;
        quint64 revision;
        QVariantList operations;
    };
    QList<SnapshotRequest> snapshots;
    QList<CommitRequest> commits;
};

QVariantMap defaultAppearanceMap()
{
    return AppearanceValues{}.toVariantMap();
}

QVariantMap snapshotWire(const QString &epoch, quint64 revision,
                         const QVariantMap &values)
{
    QVariantMap sources;
    const auto keys = AppearanceKeys::scopedKeys();
    for (const auto &key : keys) {
        sources.insert(key, QStringLiteral("user-overrides"));
    }
    return {{QLatin1StringView(WireContract::FieldStatus),
             quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion),
             quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), epoch},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

QVariantMap commitWire(SettingsWireStatus status, quint64 before, quint64 after,
                       const QVariantMap &currentValues, const QString &epoch,
                       const QString &message = {})
{
    QStringList changed;
    if (status == SettingsWireStatus::Applied && after == before + 1) {
        for (auto it = currentValues.constBegin(); it != currentValues.constEnd(); ++it) {
            changed.append(it.key());
        }
    }
    // Known-key outcomes must retain exactly one value/source entry for the
    // operated key; UnknownKey alone carries the empty pair.
    QVariantMap sources;
    for (auto it = currentValues.constBegin(); it != currentValues.constEnd(); ++it) {
        sources.insert(it.key(), QStringLiteral("user-overrides"));
    }
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(status)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion),
             quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), epoch},
            {QLatin1StringView(WireContract::FieldRevisionBefore), before},
            {QLatin1StringView(WireContract::FieldRevisionAfter), after},
            {QLatin1StringView(WireContract::FieldValues), currentValues},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources},
            {QLatin1StringView(WireContract::FieldChangedKeys), changed},
            {QLatin1StringView(WireContract::FieldMessage), message}};
}

QVector<QindaQt::Themes::ThemeSpec> loadFixtureThemes()
{
    QVector<QindaQt::Themes::ThemeSpec> themes;
    for (const auto *file : {"/data/themes/qinda-dark.json",
                             "/data/themes/qinda-light.json",
                             "/data/themes/qinda-high-contrast.json"}) {
        const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
            QStringLiteral(QINDAQT_SOURCE_DIR) + QLatin1String(file));
        if (loaded.ok) {
            themes.append(loaded.theme);
        }
    }
    return themes;
}

// Establish one authoritative baseline: owner plus one answered snapshot.
// QTest macros expand `return;`, so these helpers report their outcome as a
// bool and every caller asserts it before touching the model.
[[nodiscard]] bool establishBaseline(SequenceTransport &transport,
                                     const QString &owner, const QString &epoch,
                                     quint64 revision, const QVariantMap &values)
{
    Q_EMIT transport.ownerChanged(owner);
    if (!QTest::qWaitFor([&transport]() { return !transport.snapshots.isEmpty(); },
                         5'000)) {
        return false;
    }
    const auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(epoch, revision, values));
    return true;
}

// Answer the automatic post-commit refresh so the client returns to Ready.
[[nodiscard]] bool answerRefresh(SequenceTransport &transport,
                                 const QString &epoch, quint64 revision,
                                 const QVariantMap &values)
{
    if (!QTest::qWaitFor([&transport]() { return !transport.snapshots.isEmpty(); },
                         5'000)) {
        return false;
    }
    const auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(epoch, revision, values));
    return true;
}

} // namespace

class AppearanceSettingsModelTests final : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void loadingThenReadyWithConfirmedBaseline();
    void draftValidationGatesApplyAndCancelRestores();
    void applySequencesPerKeyCommitsInOrder();
    void conflictStopsSequenceAndRequiresExplicitReapply();
    void uncertainWriteIsNeverReplayed();
    void ownerLossDuringSequenceAbortsWithoutReplay();
    void ownerReplacementBetweenReplyAndSnapshotDoesNotReplay();
    void confirmedFailureDiagnosticSurvivesRebaseline();
    void invalidSnapshotTypeFailsClosed();

private:
    // AGENT-GUARD: QTest macros expand `return;`, so this helper reports
    // failure through the returned pointer and every caller must assert it
    // before dereferencing. The member client is bound to the member
    // transport, so makeModel-based tests drive m_transport only, and init()
    // deletes the prior model BEFORE stopping the shared client so no stale
    // model stays connected across slots. Construction order is
    // transport → client → model; init() reverses it explicitly.
    // The member client ignores same-owner replacements, so every makeModel
    // call must present a fresh unique owner or no baseline is ever fetched.
    [[nodiscard]] AppearanceSettingsModel *makeModel(Qt::ColorScheme scheme)
    {
        auto *model = new AppearanceSettingsModel(
            m_client, loadFixtureThemes(), scheme, nullptr, this);
        if (!m_client.start()) {
            delete model;
            return nullptr;
        }
        ++m_ownerSequence;
        const QString owner = QStringLiteral(":1.5%1").arg(m_ownerSequence);
        if (!establishBaseline(m_transport, owner, QStringLiteral("epoch-a"),
                               7, defaultAppearanceMap())) {
            delete model;
            return nullptr;
        }
        if (!QTest::qWaitFor([model]() { return model->ready(); }, 5'000)) {
            delete model;
            return nullptr;
        }
        m_model = model;
        return model;
    }

    SequenceTransport m_transport;
    int m_ownerSequence = 0;
    AppearanceSettingsModel *m_model = nullptr;
    SettingsClient m_client{m_transport, AppearanceKeys::scopedKeys(),
                            {.requestTimeoutMilliseconds = 100,
                             .debounceMilliseconds = 0,
                             .retryMilliseconds = {10}}};
};

void AppearanceSettingsModelTests::init()
{
    // Reverse construction order: the model dies while the client is still
    // alive and connected, so no slot fires into a dangling model.
    delete m_model;
    m_model = nullptr;
    m_client.stop();
    m_transport.snapshots.clear();
    m_transport.commits.clear();
}

void AppearanceSettingsModelTests::cleanup()
{
    // The final slot needs the same reverse-order teardown as every slot;
    // relying on the test object's QObject child cleanup would destroy the
    // client member before its model child.
    delete m_model;
    m_model = nullptr;
    m_client.stop();
    m_transport.snapshots.clear();
    m_transport.commits.clear();
}

void AppearanceSettingsModelTests::loadingThenReadyWithConfirmedBaseline()
{
    AppearanceValues expected;
    expected.themeId = QStringLiteral("qinda-light");
    expected.fontPointSize = 11.0;
    auto values = defaultAppearanceMap();
    values[QLatin1String(AppearanceKeys::Theme)] = expected.themeId;
    values[QLatin1String(AppearanceKeys::FontPointSize)] = expected.fontPointSize;

    SequenceTransport transport;
    SettingsClient client(transport, AppearanceKeys::scopedKeys(),
                          {.requestTimeoutMilliseconds = 100,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    AppearanceSettingsModel model(client, loadFixtureThemes(),
                                  Qt::ColorScheme::Dark);
    QVERIFY(model.loading());
    QVERIFY(!model.canEdit());
    QVERIFY(client.start());
    QVERIFY(establishBaseline(transport, QStringLiteral(":1.41"),
                              QStringLiteral("epoch"), 3, values));
    QTRY_VERIFY(model.ready());
    QVERIFY(model.canEdit());
    QVERIFY(!model.draftDirty());
    QVERIFY(model.draftValid());
    QCOMPARE(model.draft().value(QLatin1String(AppearanceKeys::Theme)),
             expected.themeId);
    QCOMPARE(model.draft().value(QLatin1String(AppearanceKeys::FontPointSize)),
             expected.fontPointSize);
    QCOMPARE(model.resolvedThemeId(), QStringLiteral("qinda-light"));
    QVERIFY(model.configuredThemeInstalled());
    QCOMPARE(model.fallbackNotice(), QString{});
    QVERIFY(model.statusText().isEmpty());
}

void AppearanceSettingsModelTests::draftValidationGatesApplyAndCancelRestores()
{
    auto *model = makeModel(Qt::ColorScheme::Light);
    QVERIFY(model != nullptr);

    QVERIFY(!model->setDraftValue(QStringLiteral("appearance.unknown"), 1));
    QVERIFY(!model->setDraftValue(QLatin1String(AppearanceKeys::UiScale),
                                  QStringLiteral("bogus")));
    QVERIFY(!model->setDraftValue(QLatin1String(AppearanceKeys::FontPointSize),
                                  QStringLiteral("12")));

    QVERIFY(model->setDraftValue(QLatin1String(AppearanceKeys::FontPointSize),
                                 99.0));
    QVERIFY(model->draftDirty());
    QVERIFY(!model->draftValid());
    QVERIFY(model->fieldErrors().contains(
        QLatin1String(AppearanceKeys::FontPointSize)));
    QVERIFY(!model->applyAvailable());
    QVERIFY(!model->applyDraft());

    QVERIFY(model->setDraftValue(QLatin1String(AppearanceKeys::Theme),
                                 QStringLiteral("not-installed")));
    QVERIFY(!model->draftValid());
    QVERIFY(model->fieldErrors().contains(QLatin1String(AppearanceKeys::Theme)));

    QVERIFY(model->cancelDraft());
    QVERIFY(!model->draftDirty());
    QVERIFY(model->draftValid());
    QCOMPARE(model->draft().value(QLatin1String(AppearanceKeys::Theme)),
             QStringLiteral("qinda-dark"));
    // Cancel without dirt or outside Ready is refused, not silently ignored.
    QVERIFY(!model->cancelDraft());
}

void AppearanceSettingsModelTests::applySequencesPerKeyCommitsInOrder()
{
    auto *model = makeModel(Qt::ColorScheme::Light);
    QVERIFY(model != nullptr);

    QVERIFY(model->setDraftValue(QLatin1String(AppearanceKeys::Theme),
                                 QStringLiteral("qinda-light")));
    QVERIFY(model->setDraftValue(QLatin1String(AppearanceKeys::FontPointSize),
                                 12.0));
    QVERIFY(model->applyDraft());
    QVERIFY(model->saving());

    // First queued key commits alone; the public client writes one key.
    QCOMPARE(m_transport.commits.size(), 1);
    const auto first = m_transport.commits.constFirst();
    QCOMPARE(first.operations.size(), 1);
    const auto firstOperation = first.operations.first().toMap();
    QCOMPARE(firstOperation.value(QLatin1StringView(WireContract::FieldKey))
                 .toString(),
             QStringLiteral("appearance.theme"));
    QCOMPARE(firstOperation.value(QLatin1StringView(WireContract::FieldValue))
                 .toString(),
             QStringLiteral("qinda-light"));

    auto values = defaultAppearanceMap();
    values[QLatin1String(AppearanceKeys::Theme)] = QStringLiteral("qinda-light");
    Q_EMIT m_transport.commitReceived(
        first.token, first.owner,
        commitWire(SettingsWireStatus::Applied, 7, 8,
                   {{QLatin1String(AppearanceKeys::Theme),
                     QStringLiteral("qinda-light")}},
                   QStringLiteral("epoch-a")));
    QVERIFY(answerRefresh(m_transport, QStringLiteral("epoch-a"),
                           8, values));

    // Only after fresh authority does the second key go out.
    QTRY_COMPARE(m_transport.commits.size(), 2);
    const auto second = m_transport.commits.constLast();
    const auto secondOperation = second.operations.first().toMap();
    QCOMPARE(secondOperation.value(QLatin1StringView(WireContract::FieldKey))
                 .toString(),
             QStringLiteral("fonts.pointSize"));
    QCOMPARE(secondOperation.value(QLatin1StringView(WireContract::FieldValue))
                 .toDouble(),
             12.0);

    values[QLatin1String(AppearanceKeys::FontPointSize)] = 12.0;
    Q_EMIT m_transport.commitReceived(
        second.token, second.owner,
        commitWire(SettingsWireStatus::Applied, 8, 9,
                   {{QLatin1String(AppearanceKeys::FontPointSize), 12.0}},
                   QStringLiteral("epoch-a")));
    QVERIFY(answerRefresh(m_transport, QStringLiteral("epoch-a"), 9, values));

    QTRY_VERIFY(model->ready());
    QVERIFY(!model->draftDirty());
    QVERIFY(!model->saving());
}

void AppearanceSettingsModelTests::conflictStopsSequenceAndRequiresExplicitReapply()
{
    auto *model = makeModel(Qt::ColorScheme::Light);
    QVERIFY(model != nullptr);

    QVERIFY(model->setDraftValue(QLatin1String(AppearanceKeys::Theme),
                                 QStringLiteral("qinda-light")));
    QVERIFY(model->setDraftValue(QLatin1String(AppearanceKeys::FontPointSize),
                                 13.0));
    QVERIFY(model->applyDraft());
    QCOMPARE(m_transport.commits.size(), 1);
    const auto first = m_transport.commits.constFirst();

    // Someone else changed the theme meanwhile; the reply says Conflict with
    // the authoritative current value and the conflicting revision.
    auto authority = defaultAppearanceMap();
    authority[QLatin1String(AppearanceKeys::Theme)] =
        QStringLiteral("qinda-high-contrast");
    Q_EMIT m_transport.commitReceived(
        first.token, first.owner,
        commitWire(SettingsWireStatus::Conflict, 8, 8,
                   {{QLatin1String(AppearanceKeys::Theme),
                     QStringLiteral("qinda-high-contrast")}},
                   QStringLiteral("epoch-a")));
    // Conflict intent remains visible through the routine reply-to-snapshot
    // transition; it is not editable against authority until that fresh
    // snapshot arrives.
    QTRY_VERIFY(model->conflict());
    QVERIFY(!model->canEdit());
    QCOMPARE(m_transport.commits.size(), 1);

    QVERIFY(answerRefresh(m_transport, QStringLiteral("epoch-a"), 8, authority));
    QTRY_VERIFY(model->conflict());
    QVERIFY(model->canEdit());
    QVERIFY(model->applyAvailable());
    // Fresh authority still differs from the draft: conflict intent survives.
    QCOMPARE(model->errorText(), QString{});
    QCOMPARE(model->statusText(),
             QStringLiteral("Appearance changed elsewhere; current values reloaded"));

    // Re-apply is explicit user intent against the fresh baseline.
    QVERIFY(model->applyDraft());
    QCOMPARE(m_transport.commits.size(), 2);
    const auto retry = m_transport.commits.constLast();
    QCOMPARE(retry.operations.first().toMap()
                 .value(QLatin1StringView(WireContract::FieldKey))
                 .toString(),
             QStringLiteral("appearance.theme"));
    authority[QLatin1String(AppearanceKeys::Theme)] =
        QStringLiteral("qinda-light");
    Q_EMIT m_transport.commitReceived(
        retry.token, retry.owner,
        commitWire(SettingsWireStatus::Applied, 8, 9,
                   {{QLatin1String(AppearanceKeys::Theme),
                     QStringLiteral("qinda-light")}},
                   QStringLiteral("epoch-a")));
    QVERIFY(answerRefresh(m_transport, QStringLiteral("epoch-a"), 9, authority));
    QTRY_COMPARE(m_transport.commits.size(), 3);
    const auto scaleCommit = m_transport.commits.constLast();
    Q_EMIT m_transport.commitReceived(
        scaleCommit.token, scaleCommit.owner,
        commitWire(SettingsWireStatus::Applied, 9, 10,
                   {{QLatin1String(AppearanceKeys::FontPointSize), 13.0}},
                   QStringLiteral("epoch-a")));
    authority[QLatin1String(AppearanceKeys::FontPointSize)] = 13.0;
    QVERIFY(answerRefresh(m_transport, QStringLiteral("epoch-a"), 10, authority));
    QTRY_VERIFY(model->ready());
    QVERIFY(!model->draftDirty());
}

void AppearanceSettingsModelTests::uncertainWriteIsNeverReplayed()
{
    auto *model = makeModel(Qt::ColorScheme::Light);
    QVERIFY(model != nullptr);

    QVERIFY(model->setDraftValue(QLatin1String(AppearanceKeys::Theme),
                                 QStringLiteral("qinda-light")));
    QVERIFY(model->applyDraft());
    QCOMPARE(m_transport.commits.size(), 1);
    const auto commit = m_transport.commits.constFirst();

    // Timeout/transport loss during the write is classified uncertain.
    Q_EMIT m_transport.requestFailed(commit.token, commit.owner,
                                    QStringLiteral("org.freedesktop.DBus.Error.Timeout"),
                                    QStringLiteral("reply timed out"));
    QTRY_VERIFY(model->unavailable());
    QCOMPARE(model->statusText(),
             QStringLiteral("Last confirmed appearance settings retained; refresh to continue"));
    QVERIFY(!model->errorText().isEmpty());
    QCOMPARE(m_transport.commits.size(), 1);

    // The client already refreshes authority after an uncertain write.
    QVERIFY(answerRefresh(m_transport, QStringLiteral("epoch-a"), 7,
                          defaultAppearanceMap()));
    QTRY_VERIFY(model->ready());
    QCOMPARE(m_transport.commits.size(), 1);

    // An explicit Retry is a plain refresh, never a resubmit.
    model->retry();
    QVERIFY(answerRefresh(m_transport, QStringLiteral("epoch-a"), 7,
                          defaultAppearanceMap()));
    QTRY_VERIFY(model->ready());
    QCOMPARE(m_transport.commits.size(), 1);

    // Only an explicit new Apply resubmits after an uncertain outcome.
    QVERIFY(model->applyDraft());
    QCOMPARE(m_transport.commits.size(), 2);
}

void AppearanceSettingsModelTests::ownerLossDuringSequenceAbortsWithoutReplay()
{
    auto *model = makeModel(Qt::ColorScheme::Light);
    QVERIFY(model != nullptr);

    QVERIFY(model->setDraftValue(QLatin1String(AppearanceKeys::Theme),
                                 QStringLiteral("qinda-light")));
    QVERIFY(model->setDraftValue(QLatin1String(AppearanceKeys::UiScale), 1.5));
    QVERIFY(model->applyDraft());
    QCOMPARE(m_transport.commits.size(), 1);
    const auto first = m_transport.commits.constFirst();

    auto values = defaultAppearanceMap();
    values[QLatin1String(AppearanceKeys::Theme)] = QStringLiteral("qinda-light");
    Q_EMIT m_transport.commitReceived(
        first.token, first.owner,
        commitWire(SettingsWireStatus::Applied, 7, 8,
                   {{QLatin1String(AppearanceKeys::Theme),
                     QStringLiteral("qinda-light")}},
                   QStringLiteral("epoch-a")));
    QVERIFY(answerRefresh(m_transport, QStringLiteral("epoch-a"),
                           8, values));
    QTRY_COMPARE(m_transport.commits.size(), 2);

    // The owner dies with the second write pending: the queued scale write
    // is dropped, not replayed behind the user's back.
    Q_EMIT m_transport.ownerChanged(QString{});
    QTRY_VERIFY(model->unavailable());
    QCOMPARE(m_transport.commits.size(), 2);
    QVERIFY(model->draftDirty());

    QVERIFY(establishBaseline(m_transport, QStringLiteral(":1.42"),
                              QStringLiteral("epoch-b"), 0, values));
    QTRY_VERIFY(model->ready());
    QCOMPARE(m_transport.commits.size(), 2);
    QVERIFY(model->draftDirty());

    QVERIFY(model->applyDraft());
    QCOMPARE(m_transport.commits.size(), 3);
}

void AppearanceSettingsModelTests::ownerReplacementBetweenReplyAndSnapshotDoesNotReplay()
{
    auto *model = makeModel(Qt::ColorScheme::Light);
    QVERIFY(model != nullptr);

    QVERIFY(model->setDraftValue(QLatin1String(AppearanceKeys::Theme),
                                 QStringLiteral("qinda-light")));
    QVERIFY(model->setDraftValue(QLatin1String(AppearanceKeys::UiScale), 1.5));
    QVERIFY(model->applyDraft());
    QCOMPARE(m_transport.commits.size(), 1);
    const auto first = m_transport.commits.constFirst();

    Q_EMIT m_transport.commitReceived(
        first.token, first.owner,
        commitWire(SettingsWireStatus::Applied, 7, 8,
                   {{QLatin1String(AppearanceKeys::Theme),
                     QStringLiteral("qinda-light")}},
                   QStringLiteral("epoch-a")));
    QVERIFY(model->saving());

    // Replacement happens after the reply but before the authoritative
    // refresh. The new lineage may confirm the first value, but must never
    // receive the queued second write without a fresh explicit Apply.
    Q_EMIT m_transport.ownerChanged(QStringLiteral(":1.99"));
    QVERIFY(answerRefresh(m_transport, QStringLiteral("epoch-b"), 0,
                          defaultAppearanceMap()));
    QTRY_VERIFY(model->ready());
    QCOMPARE(m_transport.commits.size(), 1);
    QVERIFY(model->draftDirty());
    QVERIFY(model->applyAvailable());

    QVERIFY(model->applyDraft());
    QCOMPARE(m_transport.commits.size(), 2);
    QCOMPARE(m_transport.commits.constLast().owner, QStringLiteral(":1.99"));
    QCOMPARE(m_transport.commits.constLast().epoch, QStringLiteral("epoch-b"));
}

void AppearanceSettingsModelTests::confirmedFailureDiagnosticSurvivesRebaseline()
{
    auto *model = makeModel(Qt::ColorScheme::Light);
    QVERIFY(model != nullptr);

    QVERIFY(model->setDraftValue(QLatin1String(AppearanceKeys::Wallpaper),
                                 QStringLiteral("/usr/share/wallpapers/mine.jpg")));
    QVERIFY(model->applyDraft());
    QCOMPARE(m_transport.commits.size(), 1);
    const auto commit = m_transport.commits.constFirst();

    Q_EMIT m_transport.commitReceived(
        commit.token, commit.owner,
        commitWire(SettingsWireStatus::PersistenceFailed, 7, 7,
                   {{QLatin1String(AppearanceKeys::Wallpaper), QString()}},
                   QStringLiteral("epoch-a"),
                   QStringLiteral("durable save failed")));
    // Between the reply and the client's automatic rebaseline the surface
    // shows the same last-confirmed Unavailable truth as the DND controller.
    QTRY_VERIFY(model->unavailable());
    QCOMPARE(model->errorText(), QStringLiteral("durable save failed"));
    QVERIFY(model->draftDirty());

    // Automatic rebaseline keeps the confirmed diagnostic visible.
    QVERIFY(answerRefresh(m_transport, QStringLiteral("epoch-a"), 7,
                          defaultAppearanceMap()));
    QTRY_VERIFY(model->ready());
    QCOMPARE(model->errorText(), QStringLiteral("durable save failed"));
    QCOMPARE(m_transport.commits.size(), 1);

    // A new explicit write dismisses the stale diagnostic.
    QVERIFY(model->setDraftValue(QLatin1String(AppearanceKeys::Wallpaper),
                                 QStringLiteral("")));
    QVERIFY(model->setDraftValue(QLatin1String(AppearanceKeys::Wallpaper),
                                 QStringLiteral("/usr/share/wallpapers/other.jpg")));
    QVERIFY(model->applyDraft());
    QCOMPARE(model->errorText(), QString{});
    QCOMPARE(m_transport.commits.size(), 2);
}

void AppearanceSettingsModelTests::invalidSnapshotTypeFailsClosed()
{
    SequenceTransport transport;
    SettingsClient client(transport, AppearanceKeys::scopedKeys(),
                          {.requestTimeoutMilliseconds = 100,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    AppearanceSettingsModel model(client, loadFixtureThemes(),
                                  Qt::ColorScheme::Light);
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.44"));
    QTRY_COMPARE(transport.snapshots.size(), 1);
    const auto request = transport.snapshots.takeFirst();
    auto values = defaultAppearanceMap();
    values[QLatin1String(AppearanceKeys::Theme)] = 5;
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(QStringLiteral("epoch"), 1,
                                                   values));
    QTRY_VERIFY(model.unavailable());
    QVERIFY(model.errorText().contains(QStringLiteral("appearance.theme")));
    QVERIFY(!model.canEdit());

    // Recovery returns to Ready once a decodable baseline lands.
    QVERIFY(establishBaseline(transport, QStringLiteral(":1.45"),
                              QStringLiteral("epoch-c"), 2, defaultAppearanceMap()));
    QTRY_VERIFY(model.ready());
}

QTEST_GUILESS_MAIN(AppearanceSettingsModelTests)
#include "tst_appearance_settings_model.moc"
