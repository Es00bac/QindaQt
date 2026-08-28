// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_settings_model.h"
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

class AdversarialTransport final : public SettingsTransport {
    Q_OBJECT
public:
    bool start(QString *) override { return true; }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner,
                         const QStringList &) override
    {
        snapshots.append({token, owner});
    }
    void commit(quint64 token, const QString &owner, const QString &epoch,
                quint64 revision, const QVariantList &operations) override
    {
        commits.append({token, owner, epoch, revision, operations});
    }
    void requestActivation() override {}

    struct SnapshotRequest final { quint64 token; QString owner; };
    struct CommitRequest final {
        quint64 token;
        QString owner;
        QString epoch;
        quint64 revision;
        QVariantList operations;
    };
    QList<SnapshotRequest> snapshots;
    QList<CommitRequest> commits;
};

QVariantMap baselineValues()
{
    return AppearanceValues{}.toVariantMap();
}

QVariantMap snapshotWire(const QString &epoch, quint64 revision,
                         const QVariantMap &values)
{
    QVariantMap sources;
    for (const QString &key : AppearanceKeys::scopedKeys()) {
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
                       const QString &key, const QVariant &value,
                       const QString &epoch, const QString &message = {})
{
    const QVariantMap current{{key, value}};
    const QVariantMap sources{{key, QStringLiteral("user-overrides")}};
    const QStringList changed = status == SettingsWireStatus::Applied
        ? QStringList{key} : QStringList{};
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(status)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion),
             quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), epoch},
            {QLatin1StringView(WireContract::FieldRevisionBefore), before},
            {QLatin1StringView(WireContract::FieldRevisionAfter), after},
            {QLatin1StringView(WireContract::FieldValues), current},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources},
            {QLatin1StringView(WireContract::FieldChangedKeys), changed},
            {QLatin1StringView(WireContract::FieldMessage), message}};
}

QVector<QindaQt::Themes::ThemeSpec> fixtureThemes()
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

[[nodiscard]] bool answerSnapshot(AdversarialTransport &transport,
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

[[nodiscard]] QString operationKey(
    const AdversarialTransport::CommitRequest &request)
{
    return request.operations.first().toMap()
        .value(QLatin1StringView(WireContract::FieldKey)).toString();
}

} // namespace

class AppearanceSettingsModelAdversarialTests final : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void rejectsConvertibleMetatypesForEveryEnumField();
    void conflictRevertReturnsToCleanReady();
    void freshAuthorityRebasesOnlyUntouchedFields();
    void laterKeyFailureNamesAppliedAndNotAttemptedResults();

private:
    [[nodiscard]] AppearanceSettingsModel *makeModel()
    {
        auto *model = new AppearanceSettingsModel(
            m_client, fixtureThemes(), Qt::ColorScheme::Light, nullptr, this);
        if (!m_client.start()) {
            delete model;
            return nullptr;
        }
        ++m_ownerSequence;
        m_owner = QStringLiteral(":1.8%1").arg(m_ownerSequence);
        Q_EMIT m_transport.ownerChanged(m_owner);
        if (!answerSnapshot(m_transport, QStringLiteral("epoch-a"), 7,
                            baselineValues())) {
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

    AdversarialTransport m_transport;
    SettingsClient m_client{m_transport, AppearanceKeys::scopedKeys(),
                            {.requestTimeoutMilliseconds = 100,
                             .debounceMilliseconds = 0,
                             .retryMilliseconds = {10}}};
    AppearanceSettingsModel *m_model = nullptr;
    int m_ownerSequence = 0;
    QString m_owner;
};

void AppearanceSettingsModelAdversarialTests::init()
{
    delete m_model;
    m_model = nullptr;
    m_client.stop();
    m_transport.snapshots.clear();
    m_transport.commits.clear();
}

void AppearanceSettingsModelAdversarialTests::cleanup()
{
    init();
}

void AppearanceSettingsModelAdversarialTests::rejectsConvertibleMetatypesForEveryEnumField()
{
    auto *model = makeModel();
    QVERIFY(model != nullptr);
    const QVariantMap before = model->draft();
    for (const auto key : {AppearanceKeys::ColorScheme,
                           AppearanceKeys::FontHinting,
                           AppearanceKeys::FontSubpixelOrder,
                           AppearanceKeys::WallpaperMode}) {
        QVERIFY(!model->setDraftValue(QLatin1String(key), 1));
        QVERIFY(!model->setDraftValue(QLatin1String(key), true));
    }
    QCOMPARE(model->draft(), before);
    QVERIFY(!model->draftDirty());
}

void AppearanceSettingsModelAdversarialTests::conflictRevertReturnsToCleanReady()
{
    auto *model = makeModel();
    QVERIFY(model != nullptr);
    QVERIFY(model->setDraftValue(QLatin1String(AppearanceKeys::Theme),
                                 QStringLiteral("qinda-light")));
    QVERIFY(model->applyDraft());
    QCOMPARE(m_transport.commits.size(), 1);
    const auto commit = m_transport.commits.constFirst();

    auto authority = baselineValues();
    authority[QLatin1String(AppearanceKeys::Theme)] =
        QStringLiteral("qinda-high-contrast");
    Q_EMIT m_transport.commitReceived(
        commit.token, commit.owner,
        commitWire(SettingsWireStatus::Conflict, 8, 8,
                   QLatin1String(AppearanceKeys::Theme),
                   QStringLiteral("qinda-high-contrast"),
                   QStringLiteral("epoch-a")));
    QVERIFY(answerSnapshot(m_transport, QStringLiteral("epoch-a"), 8, authority));
    QTRY_VERIFY(model->conflict());
    QVERIFY(model->canEdit());
    QCOMPARE(model->saveResults().first().toMap().value(QStringLiteral("state")),
             QStringLiteral("conflict"));

    QVERIFY(model->cancelDraft());
    QVERIFY(model->ready());
    QVERIFY(!model->conflict());
    QVERIFY(!model->draftDirty());
    QCOMPARE(model->statusText(), QString{});
    QCOMPARE(model->errorText(), QString{});
    QVERIFY(model->saveResults().isEmpty());
    QCOMPARE(m_transport.commits.size(), 1);
    QCOMPARE(model->draft().value(QLatin1String(AppearanceKeys::Theme)),
             QStringLiteral("qinda-high-contrast"));
}

void AppearanceSettingsModelAdversarialTests::freshAuthorityRebasesOnlyUntouchedFields()
{
    auto *model = makeModel();
    QVERIFY(model != nullptr);

    auto authority = baselineValues();
    authority[QLatin1String(AppearanceKeys::Wallpaper)] =
        QStringLiteral("/external/clean.jpg");
    Q_EMIT m_transport.settingsChanged(m_owner, QStringLiteral("epoch-a"), 8,
                                      {QLatin1String(AppearanceKeys::Wallpaper)});
    QVERIFY(answerSnapshot(m_transport, QStringLiteral("epoch-a"), 8, authority));
    QTRY_VERIFY(model->ready());
    QVERIFY(!model->draftDirty());
    QCOMPARE(model->draft().value(QLatin1String(AppearanceKeys::Wallpaper)),
             QStringLiteral("/external/clean.jpg"));

    authority[QLatin1String(AppearanceKeys::Wallpaper)] =
        QStringLiteral("/replacement/clean.jpg");
    m_owner = QStringLiteral(":1.999");
    Q_EMIT m_transport.ownerChanged(m_owner);
    QVERIFY(answerSnapshot(m_transport, QStringLiteral("epoch-b"), 0, authority));
    QTRY_VERIFY(model->ready());
    QVERIFY(!model->draftDirty());
    QCOMPARE(model->draft().value(QLatin1String(AppearanceKeys::Wallpaper)),
             QStringLiteral("/replacement/clean.jpg"));

    QVERIFY(model->setDraftValue(QLatin1String(AppearanceKeys::Theme),
                                 QStringLiteral("qinda-light")));
    authority[QLatin1String(AppearanceKeys::Wallpaper)] =
        QStringLiteral("/external/while-editing.jpg");
    Q_EMIT m_transport.settingsChanged(m_owner, QStringLiteral("epoch-b"), 1,
                                      {QLatin1String(AppearanceKeys::Wallpaper)});
    QVERIFY(answerSnapshot(m_transport, QStringLiteral("epoch-b"), 1, authority));
    QTRY_VERIFY(model->ready());
    QCOMPARE(model->draft().value(QLatin1String(AppearanceKeys::Theme)),
             QStringLiteral("qinda-light"));
    QCOMPARE(model->draft().value(QLatin1String(AppearanceKeys::Wallpaper)),
             QStringLiteral("/external/while-editing.jpg"));
    QVERIFY(model->applyDraft());
    QCOMPARE(m_transport.commits.size(), 1);
    QCOMPARE(operationKey(m_transport.commits.constFirst()),
             QLatin1String(AppearanceKeys::Theme));
}

void AppearanceSettingsModelAdversarialTests::laterKeyFailureNamesAppliedAndNotAttemptedResults()
{
    auto *model = makeModel();
    QVERIFY(model != nullptr);
    QVERIFY(model->setDraftValue(QLatin1String(AppearanceKeys::Theme),
                                 QStringLiteral("qinda-light")));
    QVERIFY(model->setDraftValue(QLatin1String(AppearanceKeys::FontPointSize),
                                 12.0));
    QVERIFY(model->setDraftValue(QLatin1String(AppearanceKeys::UiScale), 1.5));
    QVERIFY(model->applyDraft());

    const auto first = m_transport.commits.constFirst();
    auto authority = baselineValues();
    authority[QLatin1String(AppearanceKeys::Theme)] = QStringLiteral("qinda-light");
    Q_EMIT m_transport.commitReceived(
        first.token, first.owner,
        commitWire(SettingsWireStatus::Applied, 7, 8,
                   QLatin1String(AppearanceKeys::Theme),
                   QStringLiteral("qinda-light"), QStringLiteral("epoch-a")));
    QVERIFY(answerSnapshot(m_transport, QStringLiteral("epoch-a"), 8, authority));
    QTRY_COMPARE(m_transport.commits.size(), 2);

    const auto second = m_transport.commits.constLast();
    QCOMPARE(operationKey(second), QLatin1String(AppearanceKeys::FontPointSize));
    Q_EMIT m_transport.commitReceived(
        second.token, second.owner,
        commitWire(SettingsWireStatus::PersistenceFailed, 8, 8,
                   QLatin1String(AppearanceKeys::FontPointSize), 10.0,
                   QStringLiteral("epoch-a"), QStringLiteral("disk full")));
    QVERIFY(answerSnapshot(m_transport, QStringLiteral("epoch-a"), 8, authority));
    QTRY_VERIFY(model->ready());
    QCOMPARE(m_transport.commits.size(), 2);

    const QVariantList results = model->saveResults();
    QCOMPARE(results.size(), 3);
    QCOMPARE(results.at(0).toMap().value(QStringLiteral("key")),
             QLatin1String(AppearanceKeys::Theme));
    QCOMPARE(results.at(0).toMap().value(QStringLiteral("state")),
             QStringLiteral("applied"));
    QCOMPARE(results.at(1).toMap().value(QStringLiteral("state")),
             QStringLiteral("failed"));
    QCOMPARE(results.at(1).toMap().value(QStringLiteral("message")),
             QStringLiteral("disk full"));
    QCOMPARE(results.at(2).toMap().value(QStringLiteral("key")),
             QLatin1String(AppearanceKeys::UiScale));
    QCOMPARE(results.at(2).toMap().value(QStringLiteral("state")),
             QStringLiteral("not-attempted"));
    QVERIFY(model->saveResultsHaveFailure());
    QVERIFY(model->saveResultsText().contains(QStringLiteral("appearance.theme — Applied")));
    QVERIFY(model->saveResultsText().contains(QStringLiteral("fonts.pointSize — Failed")));
    QVERIFY(model->saveResultsText().contains(QStringLiteral("appearance.uiScale — Not attempted")));
}

int runAppearanceSettingsModelAdversarialTests(int argc, char **argv)
{
    AppearanceSettingsModelAdversarialTests adversarial;
    return QTest::qExec(&adversarial, argc, argv);
}

#include "tst_appearance_settings_model_adversarial.moc"
