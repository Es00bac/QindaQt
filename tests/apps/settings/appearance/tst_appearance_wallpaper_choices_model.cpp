// SPDX-License-Identifier: GPL-3.0-or-later
// ADR-0286: per-display and per-desktop wallpaper choices in the Appearance
// route model — scoped draft edits, the precedence the preview shows, one
// strict Settings1 key on Apply, Revert, saved choices for absent displays
// and desktops, and an unreadable confirmed value that is reported and then
// replaced by the next explicit choice.
#include "qindaqt/apps/settings_appearance/appearance_settings_model.h"
#include "qindaqt/apps/settings_appearance/appearance_values.h"
#include "qindaqt/apps/settings_appearance/wallpaper_target_catalog.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/wallpaper_assignments/wallpaper_assignments.h"
#include "qindaqt/themes/theme_loader.h"

#include <QSignalSpy>
#include <QtTest>

#include <memory>

using namespace QindaQt::Apps::SettingsAppearance;
using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;
using QindaQt::Services::WallpaperAssignments::WallpaperAssignments;

namespace {

const QString kLeft = QStringLiteral("edid:00112233445566778899aabbccddeeff");
const QString kGone = QStringLiteral("edid:ffeeddccbbaa99887766554433221100");
const QString kWork = QStringLiteral("desktop-work");
const QString kKey = QString(AppearanceKeys::WallpaperAssignments);

class ChoiceTransport final : public SettingsTransport {
    Q_OBJECT
public:
    bool start(QString *) override { return true; }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner, const QStringList &) override
    {
        snapshots.append({token, owner});
    }
    void commit(quint64 token, const QString &owner, const QString &, quint64,
                const QVariantList &operations) override
    {
        commits.append({token, owner, operations});
    }
    void requestActivation() override {}

    struct Request final {
        quint64 token;
        QString owner;
    };
    struct Commit final {
        quint64 token;
        QString owner;
        QVariantList operations;
    };
    QList<Request> snapshots;
    QList<Commit> commits;
};

QVariantMap baseline()
{
    QVariantMap values = AppearanceValues{}.toVariantMap();
    values.insert(QString(AppearanceKeys::Wallpaper), QStringLiteral("qindaqt:jade-fold"));
    return values;
}

QVariantMap snapshotWire(quint64 revision, const QVariantMap &values)
{
    QVariantMap sources;
    for (const QString &key : AppearanceKeys::scopedKeys())
        sources.insert(key, QStringLiteral("user-overrides"));
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch-w")},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

QVariantMap commitWire(quint64 before, const QString &key, const QVariant &value)
{
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch-w")},
            {QLatin1StringView(WireContract::FieldRevisionBefore), before},
            {QLatin1StringView(WireContract::FieldRevisionAfter), before + 1},
            {QLatin1StringView(WireContract::FieldValues), QVariantMap{{key, value}}},
            {QLatin1StringView(WireContract::FieldSourceLayers),
             QVariantMap{{key, QStringLiteral("user-overrides")}}},
            {QLatin1StringView(WireContract::FieldChangedKeys), QStringList{key}},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

QVector<QindaQt::Themes::ThemeSpec> fixtureThemes()
{
    QVector<QindaQt::Themes::ThemeSpec> themes;
    const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    if (loaded.ok)
        themes.append(loaded.theme);
    return themes;
}

QVariantList bundled()
{
    return {QVariantMap{{QStringLiteral("name"), QStringLiteral("Neon harbor")},
                        {QStringLiteral("value"), QStringLiteral("qindaqt:neon-harbor")},
                        {QStringLiteral("previewUrl"),
                         QUrl::fromLocalFile(QStringLiteral("/nonexistent/neon-harbor.png"))}}};
}

// Model-first declaration order would be wrong: the model must die before
// the client it borrows, so it is held last and released first.
struct Fixture final {
    ChoiceTransport transport;
    SettingsClient client{transport, AppearanceKeys::scopedKeys(),
                          {.requestTimeoutMilliseconds = 100,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}}};
    std::unique_ptr<AppearanceSettingsModel> model;
};

[[nodiscard]] bool answer(ChoiceTransport &transport, quint64 revision, const QVariantMap &values)
{
    if (!QTest::qWaitFor([&transport] { return !transport.snapshots.isEmpty(); }, 5'000))
        return false;
    const auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(revision, values));
    return true;
}

[[nodiscard]] bool start(Fixture &fixture, const QVariantMap &values)
{
    fixture.model = std::make_unique<AppearanceSettingsModel>(
        fixture.client, fixtureThemes(), bundled(), Qt::ColorScheme::Dark, nullptr);
    if (!fixture.client.start())
        return false;
    Q_EMIT fixture.transport.ownerChanged(QStringLiteral(":1.90"));
    return answer(fixture.transport, 7, values)
        && QTest::qWaitFor([&fixture] { return fixture.model->ready(); }, 5'000);
}

QString operationKey(const ChoiceTransport::Commit &commit)
{
    return commit.operations.constFirst().toMap()
        .value(QLatin1StringView(WireContract::FieldKey)).toString();
}

QVariant operationValue(const ChoiceTransport::Commit &commit)
{
    return commit.operations.constFirst().toMap().value(QLatin1StringView(WireContract::FieldValue));
}

} // namespace

class AppearanceWallpaperChoicesTests final : public QObject {
    Q_OBJECT

private slots:
    void scopedEditsJoinTheDraftAndApplyAfterTheEverywhereKey();
    void targetsNameSavedChoicesAndAbsentOnesStayListed();
    void unreadableConfirmedChoicesAreReportedAndReplaced();
};

void AppearanceWallpaperChoicesTests::scopedEditsJoinTheDraftAndApplyAfterTheEverywhereKey()
{
    Fixture fixture;
    QVERIFY(start(fixture, baseline()));
    AppearanceSettingsModel &model = *fixture.model;

    // Before any choice: every scope shows the everywhere wallpaper.
    QVariantMap choice = model.wallpaperChoiceFor(kLeft, kWork);
    QCOMPARE(choice.value(QStringLiteral("explicit")).toBool(), false);
    QCOMPARE(choice.value(QStringLiteral("value")).toString(), QStringLiteral("qindaqt:jade-fold"));
    QCOMPARE(choice.value(QStringLiteral("scope")).toString(), QStringLiteral("everywhere"));
    QCOMPARE(model.wallpaperChoiceFor(QString(), QString())
                 .value(QStringLiteral("explicit")).toBool(), true);

    QVERIFY(model.setWallpaperFor(kLeft, QString(), QStringLiteral("qindaqt:neon-harbor")));
    QVERIFY(model.setWallpaperFor(kLeft, kWork, QString()));
    QVERIFY(model.setWallpaperFor(QString(), QString(), QStringLiteral("qindaqt:ink-tide")));
    QVERIFY(model.draftDirty());
    choice = model.wallpaperChoiceFor(kLeft, QString());
    QCOMPARE(choice.value(QStringLiteral("scope")).toString(), QStringLiteral("display"));
    QCOMPARE(choice.value(QStringLiteral("label")).toString(), QStringLiteral("Neon harbor"));
    QCOMPARE(choice.value(QStringLiteral("previewUrl")).toUrl(),
             QUrl::fromLocalFile(QStringLiteral("/nonexistent/neon-harbor.png")));
    choice = model.wallpaperChoiceFor(kLeft, kWork);
    QCOMPARE(choice.value(QStringLiteral("explicit")).toBool(), true);
    QCOMPARE(choice.value(QStringLiteral("value")).toString(), QString());
    QCOMPARE(choice.value(QStringLiteral("label")).toString(), QStringLiteral("No wallpaper"));
    // Another display on that desktop inherits the new everywhere wallpaper.
    choice = model.wallpaperChoiceFor(kGone, kWork);
    QCOMPARE(choice.value(QStringLiteral("value")).toString(), QStringLiteral("qindaqt:ink-tide"));

    // Refusals leave the draft untouched.
    const QVariantMap before = model.draft();
    QVERIFY(!model.setWallpaperFor(QStringLiteral("edid:has space"), QString(),
                                   QStringLiteral("/x.png")));
    QVERIFY(!model.setWallpaperFor(kLeft, QString(), QStringLiteral("relative.png")));
    QVERIFY(!model.setDraftValue(kKey, QVariantMap{{QStringLiteral("version"), 2}}));
    QVERIFY(!model.clearWallpaperFor(QString(), QString()));
    QCOMPARE(model.draft(), before);

    // Revert returns to the confirmed choices (none).
    QVERIFY(model.cancelDraft());
    QVERIFY(!model.draftDirty());
    QCOMPARE(model.draft().value(kKey).toMap(), QVariantMap{});

    // Apply writes the everywhere wallpaper first, then the choices as one
    // canonical value on the key's own transaction.
    QVERIFY(model.setWallpaperFor(QString(), QString(), QStringLiteral("qindaqt:ink-tide")));
    QVERIFY(model.setWallpaperFor(kLeft, QString(), QStringLiteral("qindaqt:neon-harbor")));
    WallpaperAssignments expected;
    QCOMPARE(int(expected.set(kLeft, QString(), QStringLiteral("qindaqt:neon-harbor"))), 0);
    QVERIFY(model.applyDraft());
    QCOMPARE(fixture.transport.commits.size(), 1);
    QCOMPARE(operationKey(fixture.transport.commits.constFirst()), QString(AppearanceKeys::Wallpaper));
    auto confirmed = baseline();
    confirmed.insert(QString(AppearanceKeys::Wallpaper), QStringLiteral("qindaqt:ink-tide"));
    const auto first = fixture.transport.commits.constFirst();
    Q_EMIT fixture.transport.commitReceived(
        first.token, first.owner,
        commitWire(7, QString(AppearanceKeys::Wallpaper), QStringLiteral("qindaqt:ink-tide")));
    QVERIFY(answer(fixture.transport, 8, confirmed));
    QTRY_COMPARE(fixture.transport.commits.size(), 2);
    const auto second = fixture.transport.commits.constLast();
    QCOMPARE(operationKey(second), kKey);
    QCOMPARE(operationValue(second).toMap(), WallpaperAssignments::encodeSettingsValue(expected));
    confirmed.insert(kKey, WallpaperAssignments::encodeSettingsValue(expected));
    Q_EMIT fixture.transport.commitReceived(
        second.token, second.owner,
        commitWire(8, kKey, WallpaperAssignments::encodeSettingsValue(expected)));
    QVERIFY(answer(fixture.transport, 9, confirmed));
    QTRY_VERIFY(model.ready());
    QVERIFY(!model.draftDirty());

    // Clearing the last choice stores the schema default again.
    QVERIFY(model.clearWallpaperFor(kLeft, QString()));
    QVERIFY(model.draftDirty());
    QCOMPARE(model.draft().value(kKey).toMap(), QVariantMap{});
    QVERIFY(model.clearWallpaperFor(kLeft, QString()));
}

void AppearanceWallpaperChoicesTests::targetsNameSavedChoicesAndAbsentOnesStayListed()
{
    Fixture fixture;
    WallpaperAssignments saved;
    QCOMPARE(int(saved.set(kLeft, kWork, QStringLiteral("qindaqt:neon-harbor"))), 0);
    QCOMPARE(int(saved.set(kGone, QString(), QStringLiteral("/home/user/Pictures/Wallpapers/old.png"))), 0);
    QCOMPARE(int(saved.set(QString(), QStringLiteral("desktop-removed"), QString())), 0);
    QVariantMap values = baseline();
    values.insert(kKey, WallpaperAssignments::encodeSettingsValue(saved));
    QVERIFY(start(fixture, values));
    AppearanceSettingsModel &model = *fixture.model;
    QVERIFY(model.wallpaperAssignmentsNotice().isEmpty());

    WallpaperTargetCatalog targets;
    QindaQt::Display::Output studio;
    studio.stableId = kLeft;
    studio.connectorName = QStringLiteral("DP-1");
    studio.label = QStringLiteral("Studio");
    studio.enabled = true;
    studio.primary = true;
    studio.logicalSize = QSize(2560, 1440);
    QindaQt::Display::Output twin = studio;
    twin.stableId = QStringLiteral("edid:0123#2");
    twin.connectorName = QStringLiteral("DP-2");
    twin.ambiguousIdentity = true;
    twin.primary = false;
    twin.position = QPoint(2560, 0);
    targets.setDisplayOutputs({studio, twin});
    targets.setDesktopRows({QVariantMap{{QStringLiteral("id"), kWork},
                                        {QStringLiteral("name"), QStringLiteral("Work")}},
                            QVariantMap{{QStringLiteral("id"), QString()}}});
    QCOMPARE(targets.displays().size(), 2);
    QCOMPARE(targets.displays().constFirst().toMap().value(QStringLiteral("title")).toString(),
             QStringLiteral("Display 1"));
    QCOMPARE(targets.displays().constLast().toMap().value(QStringLiteral("assignable")).toBool(),
             false);
    QCOMPARE(targets.desktops().size(), 1);

    QSignalSpy rowsChanged(&model, &AppearanceSettingsModel::wallpaperAssignmentsChanged);
    model.setWallpaperTargets(&targets);
    QVERIFY(rowsChanged.count() >= 1);
    QVERIFY(model.wallpaperTargets() == &targets);

    const QVariantList rows = model.wallpaperAssignmentRows();
    QCOMPARE(rows.size(), 3);
    QVariantMap all;
    QVariantMap gone;
    QVariantMap studioWork;
    for (const QVariant &entry : rows) {
        const QVariantMap row = entry.toMap();
        if (row.value(QStringLiteral("display")).toString().isEmpty())
            all = row;
        else if (row.value(QStringLiteral("display")).toString() == kGone)
            gone = row;
        else
            studioWork = row;
    }
    QCOMPARE(studioWork.value(QStringLiteral("displayLabel")).toString(),
             QStringLiteral("Display 1 · Studio"));
    QCOMPARE(studioWork.value(QStringLiteral("desktopLabel")).toString(), QStringLiteral("Work"));
    QCOMPARE(studioWork.value(QStringLiteral("wallpaperLabel")).toString(),
             QStringLiteral("Neon harbor"));
    // Absent displays and desktops are named, not dropped.
    QCOMPARE(gone.value(QStringLiteral("displayPresent")).toBool(), false);
    QCOMPARE(gone.value(QStringLiteral("wallpaperLabel")).toString(), QStringLiteral("old.png"));
    QCOMPARE(all.value(QStringLiteral("desktopPresent")).toBool(), false);
    QCOMPARE(all.value(QStringLiteral("wallpaperLabel")).toString(), QStringLiteral("No wallpaper"));

    // Unplugging the display renames its row; the saved choice remains.
    const auto before = rowsChanged.count();
    targets.setDisplayOutputs({});
    QVERIFY(rowsChanged.count() > before);
    QCOMPARE(model.wallpaperAssignmentRows().size(), 3);
    QVERIFY(model.draft().value(kKey).toMap() == WallpaperAssignments::encodeSettingsValue(saved));
    QVERIFY(!model.draftDirty());

    model.setWallpaperTargets(nullptr);
    QVERIFY(model.wallpaperTargets() == nullptr);
}

void AppearanceWallpaperChoicesTests::unreadableConfirmedChoicesAreReportedAndReplaced()
{
    Fixture fixture;
    QVariantMap values = baseline();
    values.insert(kKey, QVariantMap{{QStringLiteral("version"), 99},
                                    {QStringLiteral("assignments"), QVariantList{}}});
    QVERIFY(start(fixture, values));
    AppearanceSettingsModel &model = *fixture.model;

    // The route stays usable; every display follows the everywhere wallpaper
    // and the notice says why.
    QVERIFY(model.ready());
    QVERIFY(!model.wallpaperAssignmentsNotice().isEmpty());
    QCOMPARE(model.wallpaperChoiceFor(kLeft, QString()).value(QStringLiteral("value")).toString(),
             QStringLiteral("qindaqt:jade-fold"));
    QVERIFY(!model.draftDirty());

    // An unrelated Apply never touches the unreadable value.
    QVERIFY(model.setDraftValue(QString(AppearanceKeys::WallpaperMode), QStringLiteral("tiled")));
    QVERIFY(model.applyDraft());
    QCOMPARE(fixture.transport.commits.size(), 1);
    QCOMPARE(operationKey(fixture.transport.commits.constFirst()),
             QString(AppearanceKeys::WallpaperMode));
    const auto mode = fixture.transport.commits.constFirst();
    Q_EMIT fixture.transport.commitReceived(
        mode.token, mode.owner,
        commitWire(7, QString(AppearanceKeys::WallpaperMode), QStringLiteral("tiled")));
    values.insert(QString(AppearanceKeys::WallpaperMode), QStringLiteral("tiled"));
    QVERIFY(answer(fixture.transport, 8, values));
    QTRY_VERIFY(model.ready());
    QCOMPARE(fixture.transport.commits.size(), 1);
    QVERIFY(!model.conflict());

    // The next explicit choice replaces it with a valid value.
    QVERIFY(model.setWallpaperFor(QString(), kWork, QStringLiteral("qindaqt:neon-harbor")));
    QVERIFY(model.applyDraft());
    QCOMPARE(fixture.transport.commits.size(), 2);
    QCOMPARE(operationKey(fixture.transport.commits.constLast()), kKey);
    WallpaperAssignments expected;
    QCOMPARE(int(expected.set(QString(), kWork, QStringLiteral("qindaqt:neon-harbor"))), 0);
    QCOMPARE(operationValue(fixture.transport.commits.constLast()).toMap(),
             WallpaperAssignments::encodeSettingsValue(expected));
    const auto choice = fixture.transport.commits.constLast();
    Q_EMIT fixture.transport.commitReceived(
        choice.token, choice.owner,
        commitWire(8, kKey, WallpaperAssignments::encodeSettingsValue(expected)));
    values.insert(kKey, WallpaperAssignments::encodeSettingsValue(expected));
    QVERIFY(answer(fixture.transport, 9, values));
    QTRY_VERIFY(model.wallpaperAssignmentsNotice().isEmpty());
    QTRY_VERIFY(model.ready());
}

int runAppearanceWallpaperChoicesTests(int argc, char **argv)
{
    AppearanceWallpaperChoicesTests choices;
    return QTest::qExec(&choices, argc, argv);
}

#include "tst_appearance_wallpaper_choices_model.moc"
