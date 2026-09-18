// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_test_fixtures.h"

#include "qindaqt/apps/settings_customize/customize_editor_host.h"
#include "qindaqt/profiles/profile_loader.h"
#include "qindaqt/shell_customization_editor/live_editor_host.h"
#include "qindaqt/shell_customization_editor/user_profile_store.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace QindaQt;
using namespace QindaQt::ShellCustomizationEditor;
using namespace QindaQt::ShellCustomizationEditor::TestFixtures;

namespace {

QByteArray readProfileBytes(const QString &directory, const QString &profileId)
{
    QFile file(QDir(directory).filePath(UserProfileStore::fileNameForId(profileId)));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

DropTarget target(const QString &panelId, const QString &zone,
                  const QString &beforeAppletId = {})
{
    DropTarget result;
    result.panelId = panelId;
    result.zone = zone;
    if (!beforeAppletId.isEmpty()) {
        result.beforeAppletId = beforeAppletId;
    }
    return result;
}

// Every live-customization menu entry as the intent it sends, in the order
// a user would reach for them. Both hosts run the identical list.
template <typename Host>
QVector<EditorOutcome> runMenuScript(Host &host)
{
    QVector<EditorOutcome> outcomes;
    // Applet menu: Move to end, Move left (anchor before a sibling), Remove.
    outcomes.append(host.applyGesture(MoveAppletIntent{QStringLiteral("bar"), QStringLiteral("launcher-instance")},
                                      target(QStringLiteral("bar"), QStringLiteral("end"))));
    outcomes.append(host.applyGesture(MoveAppletIntent{QStringLiteral("bar"), QStringLiteral("launcher-instance")},
                                      target(QStringLiteral("bar"), QStringLiteral("end"), QStringLiteral("clock-instance"))));
    outcomes.append(host.applyGesture(removeIntent(QStringLiteral("dock"), QStringLiteral("tasks-instance")),
                                      target(QStringLiteral("dock"), QStringLiteral("start"))));
    // Panel menu: Add applet (palette insert with the explicit instance id
    // both surfaces choose), Panel > size, Panel > edge, Add panel, Remove panel.
    outcomes.append(host.applyGesture(InsertAppletIntent{QStringLiteral("task-list")},
                                      target(QStringLiteral("dock"), QStringLiteral("center")),
                                      QStringLiteral("task-list-instance-1")));
    outcomes.append(host.applyGesture(configureIntent(QStringLiteral("bar"),
                                                      PanelConfiguration{Profiles::Layer::Above,
                                                                         Profiles::HideMode::Never, 1, 36, 1.0}),
                                      target(QStringLiteral("bar"), QStringLiteral("start"))));
    outcomes.append(host.applyGesture(movePanelIntent(QStringLiteral("bar"), QStringLiteral("*"),
                                                      Profiles::Edge::Bottom, Profiles::Alignment::Fill,
                                                      std::nullopt),
                                      target(QStringLiteral("bar"), QStringLiteral("start"))));
    Profiles::PanelSpec side;
    side.id = QStringLiteral("panel-2");
    side.output = QStringLiteral("*");
    side.edge = Profiles::Edge::Right;
    side.thickness = 32;
    outcomes.append(host.applyGesture(addPanelIntent(side, std::nullopt),
                                      target(QStringLiteral("panel-2"), QStringLiteral("start"))));
    outcomes.append(host.applyGesture(InsertAppletIntent{QStringLiteral("clock")},
                                      target(QStringLiteral("panel-2"), QStringLiteral("start")),
                                      QStringLiteral("clock-instance-2")));
    outcomes.append(host.undo());
    outcomes.append(host.applyGesture(removePanelIntent(QStringLiteral("panel-2")),
                                      target(QStringLiteral("panel-2"), QStringLiteral("start"))));
    outcomes.append(host.undo());
    // Applet settings rows: the complete map with one field changed.
    outcomes.append(host.applyGesture(configureAppletSettingsIntent(
                                          QStringLiteral("bar"), QStringLiteral("clock-instance"),
                                          QVariantMap{{QStringLiteral("zone"), QStringLiteral("end")},
                                                      {QStringLiteral("format"), QStringLiteral("24h")}}),
                                      target(QStringLiteral("bar"), QStringLiteral("end"))));
    outcomes.append(host.apply());
    return outcomes;
}

} // namespace

// The parity invariant of the live customization host: the persisted profile
// for a given intent script equals what the Settings Customize route's own
// RepositoryCustomizeEditorHost persists for the same script, byte for byte.
class LiveHostSettingsParityTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void menuScriptPersistsIdenticalBytesThroughBothHosts();
};

void LiveHostSettingsParityTest::menuScriptPersistsIdenticalBytesThroughBothHosts()
{
    QTemporaryDir liveDirectory;
    QTemporaryDir settingsDirectory;
    QVERIFY(liveDirectory.isValid() && settingsDirectory.isValid());

    LiveEditorHost live(profile(), outputs(), manifests(), liveDirectory.path());
    Apps::SettingsCustomize::RepositoryCustomizeEditorHost settings(
        profile(), outputs(), manifests(), settingsDirectory.path());
    QVERIFY2(live.ready(), qPrintable(live.unavailableReason()));
    QVERIFY2(settings.ready(), qPrintable(settings.unavailableReason()));

    const auto liveOutcomes = runMenuScript(live);
    const auto settingsOutcomes = runMenuScript(settings);
    QCOMPARE(liveOutcomes.size(), settingsOutcomes.size());
    for (qsizetype index = 0; index < liveOutcomes.size(); ++index) {
        QVERIFY2(liveOutcomes.at(index).ok,
                 qPrintable(QStringLiteral("live step %1: %2").arg(index).arg(liveOutcomes.at(index).message)));
        QVERIFY2(settingsOutcomes.at(index).ok,
                 qPrintable(QStringLiteral("settings step %1: %2").arg(index).arg(settingsOutcomes.at(index).message)));
        QCOMPARE(liveOutcomes.at(index).code, settingsOutcomes.at(index).code);
    }

    const QByteArray liveBytes = readProfileBytes(liveDirectory.path(), QStringLiteral("editor-fixture"));
    const QByteArray settingsBytes = readProfileBytes(settingsDirectory.path(), QStringLiteral("editor-fixture"));
    QVERIFY(!liveBytes.isEmpty());
    QCOMPARE(liveBytes, settingsBytes);

    const auto loaded = Profiles::ProfileLoader::fromJson(liveBytes, QStringLiteral("parity"));
    QVERIFY2(loaded.ok, qPrintable(loaded.error.message));
    QCOMPARE(loaded.profile.panels.size(), 3);
    QVERIFY(panel(loaded.profile, QStringLiteral("bar"))->edge == Profiles::Edge::Bottom);
    QCOMPARE(panel(loaded.profile, QStringLiteral("bar"))->thickness, 36);
    QCOMPARE(appletIds(loaded.profile, QStringLiteral("bar")),
             (QStringList{QStringLiteral("launcher-instance"), QStringLiteral("clock-instance")}));
    QCOMPARE(appletIds(loaded.profile, QStringLiteral("dock")), QStringList{QStringLiteral("task-list-instance-1")});
    QCOMPARE(appletIds(loaded.profile, QStringLiteral("panel-2")), QStringList{});
    QCOMPARE(panel(loaded.profile, QStringLiteral("bar"))->applets.last().settings.value(QStringLiteral("format")).toString(),
             QStringLiteral("24h"));
}

QTEST_GUILESS_MAIN(LiveHostSettingsParityTest)
#include "tst_live_host_settings_parity.moc"
