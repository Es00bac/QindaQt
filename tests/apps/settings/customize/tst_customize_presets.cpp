// SPDX-License-Identifier: GPL-3.0-or-later
// The Settings Customize preset model (ADR-0267) over a real temporary
// catalog and user store and an in-process Settings1 double: provenance and
// order, confirmed switching, save/rename/duplicate/delete with name and
// count bounds, Modified/Restore for built-ins, and the active-preset
// deletion fallback.
#include "customize_test_support.h"

#include "qindaqt/profiles/profile_loader.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Apps::SettingsCustomize;
using namespace QindaQt::Apps::SettingsCustomize::TestSupport;
using Services::SettingsProtocol::SettingsWireStatus;

namespace {

QVariantMap presetById(const CustomizeSettingsModel &model, const QString &id)
{
    const QVariantList presets = model.presets();
    for (const QVariant &entry : presets) {
        const QVariantMap preset = entry.toMap();
        if (preset.value(QStringLiteral("id")).toString() == id) {
            return preset;
        }
    }
    return {};
}

QStringList presetIds(const CustomizeSettingsModel &model)
{
    QStringList ids;
    const QVariantList presets = model.presets();
    for (const QVariant &entry : presets) {
        ids.append(entry.toMap().value(QStringLiteral("id")).toString());
    }
    return ids;
}

QString presetIdNamed(const CustomizeSettingsModel &model, const QString &name)
{
    const QVariantList presets = model.presets();
    for (const QVariant &entry : presets) {
        const QVariantMap preset = entry.toMap();
        if (preset.value(QStringLiteral("name")).toString() == name) {
            return preset.value(QStringLiteral("id")).toString();
        }
    }
    return {};
}

Profiles::LayoutProfile readProfile(const QString &path)
{
    const auto loaded = Profiles::ProfileLoader::fromFile(path);
    return loaded.ok ? loaded.profile : Profiles::LayoutProfile{};
}

QVariant committedValue(const SequenceTransport::CommitRequest &commit)
{
    return commit.operations.isEmpty()
        ? QVariant{} : commit.operations.first().toMap().value(QStringLiteral("value"));
}

// The shell's own write shape for a direct panel edit: the whole profile
// under the same id in the user store.
Profiles::LayoutProfile editedOnThePanels(Profiles::LayoutProfile profile)
{
    profile.panels[0].thickness = 40;
    profile.panels[0].applets.removeLast();
    return profile;
}

} // namespace

class CustomizePresetsTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void builtInsComeFirstWithProvenance();
    void switchingCommitsTheSelectionAndWaitsForReadback();
    void refusedOrUncertainSwitchesKeepTheLayout();
    void saveCurrentLayoutCopiesEveryPanelEdit();
    void presetNamesAreValidatedAndBounded();
    void ownPresetsRenameDuplicateAndDelete();
    void builtInsCannotBeRenamedOrDeleted();
    void modifiedBuiltInRestoresOrSavesAsNew();
    void deletingTheActivePresetSwitchesToTheDefaultFirst();
    void failedFallbackSwitchKeepsTheActivePreset();
    void missingSelectionStaysUsable();
    void defaultPresetMatchesTheSettingsSchema();
};

void CustomizePresetsTests::builtInsComeFirstWithProvenance()
{
    ModelHarness h;
    QVERIFY(h.writeUserCopy(editedOnThePanels(profile())));
    auto own = profile(QStringLiteral("user-zebra"), QStringLiteral("Zebra"));
    QVERIFY(h.writeUserCopy(own));
    own.id = QStringLiteral("user-apple");
    own.name = QStringLiteral("Apple");
    QVERIFY(h.writeUserCopy(own));
    h.model.reloadPresets();
    QVERIFY(h.establish());

    // Built-ins in installed order, then own presets by name.
    QCOMPARE(presetIds(h.model),
             (QStringList{QStringLiteral("alternate"), QStringLiteral("fixture"),
                          QString(DefaultLayoutPresetId), QStringLiteral("user-apple"),
                          QStringLiteral("user-zebra")}));
    const QVariantMap fixture = presetById(h.model, QStringLiteral("fixture"));
    QVERIFY(fixture.value(QStringLiteral("builtIn")).toBool());
    QVERIFY(fixture.value(QStringLiteral("modified")).toBool());
    QVERIFY(!fixture.value(QStringLiteral("own")).toBool());
    QVERIFY(fixture.value(QStringLiteral("active")).toBool());
    // The card draws what the desktop would use: the edited copy.
    QCOMPARE(fixture.value(QStringLiteral("panels")).toList().first().toMap()
                 .value(QStringLiteral("thickness")).toInt(), 40);
    const QVariantMap alternate = presetById(h.model, QStringLiteral("alternate"));
    QVERIFY(alternate.value(QStringLiteral("builtIn")).toBool());
    QVERIFY(!alternate.value(QStringLiteral("modified")).toBool());
    QVERIFY(!alternate.value(QStringLiteral("active")).toBool());
    const QVariantMap zebra = presetById(h.model, QStringLiteral("user-zebra"));
    QVERIFY(zebra.value(QStringLiteral("own")).toBool());
    QVERIFY(!zebra.value(QStringLiteral("builtIn")).toBool());
    QVERIFY(!zebra.value(QStringLiteral("modified")).toBool());
    QVERIFY(presetById(h.model, QString(DefaultLayoutPresetId))
                .value(QStringLiteral("isDefault")).toBool());
    QVERIFY(!fixture.value(QStringLiteral("isDefault")).toBool());
    QCOMPARE(h.model.activePresetId(), QStringLiteral("fixture"));
    QVERIFY(h.model.canSwitch());
    QVERIFY(h.model.statusText().contains(QStringLiteral("Fixture")));
}

void CustomizePresetsTests::switchingCommitsTheSelectionAndWaitsForReadback()
{
    ModelHarness h;
    QVERIFY(h.establish());
    QVERIFY(h.model.activatePreset(QStringLiteral("fixture")));  // already active
    QCOMPARE(h.transport.commits.size(), 0);

    QVERIFY(h.model.activatePreset(QStringLiteral("alternate")));
    QCOMPARE(h.transport.commits.size(), 1);
    QCOMPARE(committedValue(h.transport.commits.first()).toString(),
             QStringLiteral("alternate"));
    QVERIFY(h.model.busy());
    QVERIFY(!h.model.canSwitch());
    QVERIFY(!h.model.canManage());
    QVERIFY(!h.model.activatePreset(QStringLiteral(u"macos-inspired")));
    QCOMPARE(h.transport.commits.size(), 1);

    QVERIFY(h.replyCommit(SettingsWireStatus::Applied, QStringLiteral("alternate")));
    // A same-lineage snapshot older than the applied revision is stale: the
    // switch stays unconfirmed and the old layout stays marked current.
    QVERIFY(h.replySnapshot(QStringLiteral("fixture"), 7));
    QVERIFY(h.model.busy());
    QCOMPARE(h.model.activePresetId(), QStringLiteral("fixture"));
    h.client.refresh();
    QVERIFY(h.replySnapshot(QStringLiteral("alternate"), 8));
    QTRY_VERIFY(!h.model.busy());
    QCOMPARE(h.model.activePresetId(), QStringLiteral("alternate"));
    QVERIFY(presetById(h.model, QStringLiteral("alternate"))
                .value(QStringLiteral("active")).toBool());
    QVERIFY(h.model.noticeText().contains(QStringLiteral("Alternate")));
    QVERIFY(h.model.errorText().isEmpty());
    QTRY_VERIFY(h.model.canSwitch());
}

void CustomizePresetsTests::refusedOrUncertainSwitchesKeepTheLayout()
{
    {
        ModelHarness h;
        QVERIFY(h.establish());
        QVERIFY(h.model.activatePreset(QStringLiteral("alternate")));
        QVERIFY(h.replyCommit(SettingsWireStatus::Conflict, QStringLiteral("fixture"),
                              QStringLiteral("changed elsewhere")));
        QVERIFY(!h.model.busy());
        QVERIFY(h.model.errorText().contains(QStringLiteral("changed elsewhere")));
        QCOMPARE(h.model.activePresetId(), QStringLiteral("fixture"));
    }
    {
        // No reply at all: the client's timeout makes the write uncertain,
        // which is reported and never replayed.
        ModelHarness h;
        QVERIFY(h.establish());
        QVERIFY(h.model.activatePreset(QStringLiteral("alternate")));
        QTRY_VERIFY_WITH_TIMEOUT(!h.model.busy(), 2'000);
        QVERIFY(h.model.errorText().contains(QStringLiteral("uncertain")));
        QCOMPARE(h.transport.commits.size(), 1);
    }
    {
        // A switch to a preset that vanished is refused before any commit.
        ModelHarness h;
        QVERIFY(h.establish());
        QVERIFY(!h.model.activatePreset(QStringLiteral("no-such-layout")));
        QCOMPARE(h.transport.commits.size(), 0);
        QVERIFY(!h.model.errorText().isEmpty());
    }
}

void CustomizePresetsTests::saveCurrentLayoutCopiesEveryPanelEdit()
{
    ModelHarness h;
    QVERIFY(h.establish());
    // The panels were edited after this page loaded; the watcher may not
    // have fired yet, so the copy must re-read the store itself.
    const Profiles::LayoutProfile edited = editedOnThePanels(profile());
    QVERIFY(h.writeUserCopy(edited));

    QVERIFY(h.model.savePresetAs(h.model.activePresetId(), QStringLiteral("  My Work  ")));
    const QString id = presetIdNamed(h.model, QStringLiteral("My Work"));
    QCOMPARE(id, QStringLiteral("user-my-work"));
    const Profiles::LayoutProfile saved = readProfile(h.userFile(id));
    QCOMPARE(saved.id, id);
    QCOMPARE(saved.name, QStringLiteral("My Work"));
    QCOMPARE(saved.panels.size(), edited.panels.size());
    QCOMPARE(saved.panels.first().thickness, 40);
    QCOMPARE(saved.panels.first().applets.size(), edited.panels.first().applets.size());
    const QVariantMap preset = presetById(h.model, id);
    QVERIFY(preset.value(QStringLiteral("own")).toBool());
    QVERIFY(!preset.value(QStringLiteral("active")).toBool());
    // Saving is not switching: the selection is untouched.
    QCOMPARE(h.transport.commits.size(), 0);
    QCOMPARE(h.model.activePresetId(), QStringLiteral("fixture"));
    QVERIFY(h.model.noticeText().contains(QStringLiteral("My Work")));

    // A second preset whose name slugs the same gets the next free id.
    QVERIFY(h.model.savePresetAs(QStringLiteral("alternate"), QStringLiteral("My-Work!")));
    QCOMPARE(presetIdNamed(h.model, QStringLiteral("My-Work!")), QStringLiteral("user-my-work-2"));
    // A name with no ASCII letters still gets a safe id.
    QVERIFY(h.model.savePresetAs(QStringLiteral("alternate"), QStringLiteral(u"日本")));
    QCOMPARE(presetIdNamed(h.model, QStringLiteral(u"日本")), QStringLiteral("user-preset"));
    // A name that slugs to a built-in id never shadows that built-in.
    QVERIFY(h.model.savePresetAs(QStringLiteral("fixture"), QStringLiteral("Alternate 2")));
    QVERIFY(!presetById(h.model, QStringLiteral("alternate"))
                 .value(QStringLiteral("modified")).toBool());
}

void CustomizePresetsTests::presetNamesAreValidatedAndBounded()
{
    ModelHarness h;
    QVERIFY(h.establish());
    const auto rejected = [&h](const QString &name) {
        const qsizetype before = h.model.presets().size();
        const bool saved = h.model.savePresetAs(QStringLiteral("fixture"), name);
        return !saved && !h.model.errorText().isEmpty()
            && h.model.presets().size() == before
            && !h.model.presetNameError(name).isEmpty();
    };
    QVERIFY(rejected(QString()));
    QVERIFY(rejected(QStringLiteral("   ")));
    QVERIFY(rejected(QString(MaximumPresetNameLength + 1, QLatin1Char('x'))));
    QVERIFY(rejected(QStringLiteral("Tab\there")));
    QVERIFY(rejected(QStringLiteral("line\nbreak")));
    // Unique against every preset, built-ins included, ignoring case.
    QVERIFY(rejected(QStringLiteral("fixture")));
    QVERIFY(rejected(QStringLiteral("  MAC ")));
    QVERIFY(!QFileInfo::exists(h.userDirectory()));

    QVERIFY(h.model.presetNameError(QString(MaximumPresetNameLength, QLatin1Char('x'))).isEmpty());
    QVERIFY(h.model.presetNameError(QStringLiteral(u"Café ☕ layout")).isEmpty());
    // Renaming may keep the preset's own name.
    QVERIFY(h.model.savePresetAs(QStringLiteral("fixture"), QStringLiteral("Mine")));
    const QString mine = presetIdNamed(h.model, QStringLiteral("Mine"));
    QVERIFY(h.model.presetNameError(QStringLiteral("mine"), mine).isEmpty());
    QVERIFY(!h.model.presetNameError(QStringLiteral("mine")).isEmpty());

    // Count bound: at most MaximumUserPresets own presets.
    for (int index = 1; index < MaximumUserPresets; ++index) {
        QVERIFY2(h.model.savePresetAs(QStringLiteral("fixture"),
                                      QStringLiteral("Preset %1").arg(index)),
                 qPrintable(h.model.errorText()));
    }
    QVERIFY(!h.model.savePresetAs(QStringLiteral("fixture"), QStringLiteral("One too many")));
    QVERIFY(h.model.errorText().contains(QString::number(MaximumUserPresets)));
    QVERIFY(!h.model.duplicatePreset(mine));
    // Edited built-ins are not own presets and do not count.
    QVERIFY(h.writeUserCopy(editedOnThePanels(profile(QStringLiteral("alternate")))));
    h.model.reloadPresets();
    QVERIFY(presetById(h.model, QStringLiteral("alternate"))
                .value(QStringLiteral("modified")).toBool());
    QVERIFY(h.model.deletePreset(mine));
    QVERIFY(h.model.savePresetAs(QStringLiteral("fixture"), QStringLiteral("Fits again")));
}

void CustomizePresetsTests::ownPresetsRenameDuplicateAndDelete()
{
    ModelHarness h;
    QVERIFY(h.establish());
    QVERIFY(h.model.savePresetAs(QStringLiteral("fixture"), QStringLiteral("Work")));
    const QString work = presetIdNamed(h.model, QStringLiteral("Work"));

    // Rename keeps the id (and file), so a selection naming it stays valid.
    QVERIFY(h.model.renamePreset(work, QStringLiteral("Office")));
    QCOMPARE(presetIdNamed(h.model, QStringLiteral("Office")), work);
    QCOMPARE(readProfile(h.userFile(work)).name, QStringLiteral("Office"));
    QVERIFY(!h.model.renamePreset(work, QStringLiteral("Alternate")));
    QVERIFY(!h.model.renamePreset(work, QString()));
    QCOMPARE(readProfile(h.userFile(work)).name, QStringLiteral("Office"));
    QVERIFY(h.model.renamePreset(work, QStringLiteral("Office")));  // unchanged: no-op

    // Duplicate: "copy", then numbered copies; the content comes along.
    QVERIFY(h.model.duplicatePreset(work));
    QVERIFY(h.model.duplicatePreset(work));
    const QString copy = presetIdNamed(h.model, QStringLiteral("Office copy"));
    const QString copy2 = presetIdNamed(h.model, QStringLiteral("Office copy 2"));
    QVERIFY(!copy.isEmpty());
    QVERIFY(!copy2.isEmpty());
    QCOMPARE(readProfile(h.userFile(copy)).panels.size(), profile().panels.size());
    // A long name still leaves room for the suffix.
    QVERIFY(h.model.renamePreset(copy2, QString(MaximumPresetNameLength, QLatin1Char('y'))));
    QVERIFY(h.model.duplicatePreset(copy2));
    const QString longCopy = QString(MaximumPresetNameLength - 5, QLatin1Char('y'))
        + QStringLiteral(" copy");
    QVERIFY(!presetIdNamed(h.model, longCopy).isEmpty());

    // Delete a preset that is not in use: the file goes, nothing is committed.
    QVERIFY(h.model.deletePreset(copy));
    QVERIFY(!QFileInfo::exists(h.userFile(copy)));
    QVERIFY(presetById(h.model, copy).isEmpty());
    QCOMPARE(h.transport.commits.size(), 0);
    QVERIFY(h.model.noticeText().contains(QStringLiteral("Office copy")));
}

void CustomizePresetsTests::builtInsCannotBeRenamedOrDeleted()
{
    ModelHarness h;
    QVERIFY(h.establish());
    QVERIFY(!h.model.renamePreset(QStringLiteral("alternate"), QStringLiteral("Renamed")));
    QVERIFY(!h.model.deletePreset(QStringLiteral("alternate")));
    QVERIFY(!h.model.duplicatePreset(QStringLiteral("alternate")));
    QVERIFY(!h.model.restorePreset(QStringLiteral("alternate")));  // not modified
    QVERIFY(!QFileInfo::exists(h.userDirectory()));
    QCOMPARE(h.transport.commits.size(), 0);
}

void CustomizePresetsTests::modifiedBuiltInRestoresOrSavesAsNew()
{
    ModelHarness h;
    QVERIFY(h.establish());
    QVERIFY(h.writeUserCopy(editedOnThePanels(profile(QStringLiteral("alternate")))));
    h.model.reloadPresets();
    QVERIFY(presetById(h.model, QStringLiteral("alternate"))
                .value(QStringLiteral("modified")).toBool());
    // Edited built-ins are not renamed or deleted, only restored or saved as new.
    QVERIFY(!h.model.renamePreset(QStringLiteral("alternate"), QStringLiteral("Other")));
    QVERIFY(!h.model.deletePreset(QStringLiteral("alternate")));

    QVERIFY(h.model.savePresetAs(QStringLiteral("alternate"), QStringLiteral("Alternate (edited)")));
    const QString kept = presetIdNamed(h.model, QStringLiteral("Alternate (edited)"));
    QVERIFY(!kept.isEmpty());
    QCOMPARE(readProfile(h.userFile(kept)).panels.first().thickness, 40);

    QVERIFY(h.model.restorePreset(QStringLiteral("alternate")));
    QVERIFY(!QFileInfo::exists(h.userFile(QStringLiteral("alternate"))));
    const QVariantMap restored = presetById(h.model, QStringLiteral("alternate"));
    QVERIFY(!restored.value(QStringLiteral("modified")).toBool());
    QCOMPARE(restored.value(QStringLiteral("panels")).toList().first().toMap()
                 .value(QStringLiteral("thickness")).toInt(),
             profile().panels.first().thickness);
    QVERIFY(h.model.noticeText().contains(QStringLiteral("Alternate")));
    // The kept copy survives the restore, and nothing touched the selection.
    QVERIFY(QFileInfo::exists(h.userFile(kept)));
    QCOMPARE(h.transport.commits.size(), 0);
}

void CustomizePresetsTests::deletingTheActivePresetSwitchesToTheDefaultFirst()
{
    ModelHarness h;
    QVERIFY(h.writeUserCopy(profile(QStringLiteral("user-mine"), QStringLiteral("Mine"))));
    h.model.reloadPresets();
    QVERIFY(h.establish(QStringLiteral("user-mine")));
    QVERIFY(presetById(h.model, QStringLiteral("user-mine")).value(QStringLiteral("active")).toBool());

    QVERIFY(h.model.deletePreset(QStringLiteral("user-mine")));
    QCOMPARE(h.transport.commits.size(), 1);
    QCOMPARE(committedValue(h.transport.commits.first()).toString(),
             QString(DefaultLayoutPresetId));
    // The file stays until the desktop has left it.
    QVERIFY(QFileInfo::exists(h.userFile(QStringLiteral("user-mine"))));
    QVERIFY(h.model.busy());

    QVERIFY(h.replyCommit(SettingsWireStatus::Applied, QString(DefaultLayoutPresetId)));
    QVERIFY(QFileInfo::exists(h.userFile(QStringLiteral("user-mine"))));
    QVERIFY(h.replySnapshot(QString(DefaultLayoutPresetId), 8));
    QTRY_VERIFY(!h.model.busy());
    QCOMPARE(h.model.activePresetId(), QString(DefaultLayoutPresetId));
    QVERIFY(!QFileInfo::exists(h.userFile(QStringLiteral("user-mine"))));
    QVERIFY(presetById(h.model, QStringLiteral("user-mine")).isEmpty());
    QVERIFY(h.model.noticeText().contains(QStringLiteral("Mine")));
    QVERIFY(h.model.noticeText().contains(QStringLiteral("Mac")));
}

void CustomizePresetsTests::failedFallbackSwitchKeepsTheActivePreset()
{
    ModelHarness h;
    QVERIFY(h.writeUserCopy(profile(QStringLiteral("user-mine"), QStringLiteral("Mine"))));
    h.model.reloadPresets();
    QVERIFY(h.establish(QStringLiteral("user-mine")));
    QVERIFY(h.model.deletePreset(QStringLiteral("user-mine")));
    QVERIFY(h.replyCommit(SettingsWireStatus::ValidationFailed, QStringLiteral("user-mine"),
                          QStringLiteral("refused")));
    QVERIFY(!h.model.busy());
    QVERIFY(QFileInfo::exists(h.userFile(QStringLiteral("user-mine"))));
    QVERIFY(h.model.errorText().contains(QStringLiteral("not deleted")));
    QCOMPARE(h.model.activePresetId(), QStringLiteral("user-mine"));
    QVERIFY(h.replySnapshot(QStringLiteral("user-mine"), 7));

    // The confirming snapshot names something else: still not deleted.
    QTRY_VERIFY(h.model.canSwitch());
    QVERIFY(h.model.deletePreset(QStringLiteral("user-mine")));
    QVERIFY(h.replyCommit(SettingsWireStatus::Applied, QString(DefaultLayoutPresetId)));
    QVERIFY(h.replySnapshot(QStringLiteral("alternate"), 8));
    QTRY_VERIFY(!h.model.busy());
    QVERIFY(QFileInfo::exists(h.userFile(QStringLiteral("user-mine"))));
    QVERIFY(h.model.errorText().contains(QStringLiteral("not deleted")));
}

void CustomizePresetsTests::missingSelectionStaysUsable()
{
    // A selection naming a layout that is gone (deleted outside Settings):
    // the shell runs its fallback, and this page must still let the user
    // choose instead of failing closed with nothing to click.
    ModelHarness h;
    QVERIFY(h.establish(QStringLiteral("deleted-elsewhere")));
    QVERIFY(h.model.ready());
    QVERIFY(h.model.statusText().contains(QStringLiteral("deleted-elsewhere")));
    for (const QVariant &entry : h.model.presets()) {
        QVERIFY(!entry.toMap().value(QStringLiteral("active")).toBool());
    }
    QVERIFY(h.model.activatePreset(QStringLiteral("alternate")));
    QCOMPARE(h.transport.commits.size(), 1);
}

void CustomizePresetsTests::defaultPresetMatchesTheSettingsSchema()
{
    // AGENT-CONTRACT (customize_settings_model.h): the deletion fallback is the
    // layout a new user gets, i.e. the schema default the shell also uses.
    QFile schema(QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"));
    QVERIFY2(schema.open(QIODevice::ReadOnly), qPrintable(schema.errorString()));
    const QJsonDocument document = QJsonDocument::fromJson(schema.readAll());
    QString schemaDefault;
    const QJsonArray keys = document.object().value(QStringLiteral("settings")).toArray();
    for (const QJsonValue &key : keys) {
        if (key.toObject().value(QStringLiteral("key")).toString()
            == QString(LayoutProfileSettingsKey)) {
            schemaDefault = key.toObject().value(QStringLiteral("default")).toString();
        }
    }
    QCOMPARE(schemaDefault, QString(DefaultLayoutPresetId));
    QVERIFY(QFileInfo::exists(QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles/")
                              + QString(DefaultLayoutPresetId) + QStringLiteral(".json")));
}

QTEST_GUILESS_MAIN(CustomizePresetsTests)
#include "tst_customize_presets.moc"
