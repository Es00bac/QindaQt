// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/shell/desktop_controls/applet_grants.h"
#include "qindaqt/shell/desktop_controls/command_search_types.h"
#include "qindaqt/shell/desktop_controls/places_controller.h"
#include "qindaqt/shell/desktop_controls/system_status_presentation.h"

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Shell::DesktopControls;

namespace {

CommandCandidate candidate(const QString &text, const QString &detail = {},
                           CommandSourceKind kind = CommandSourceKind::Applications)
{
  CommandCandidate result;
  result.kind = kind;
  result.id = commandSourceKindText(kind) + QLatin1Char(':') + text;
  result.text = text;
  result.detail = detail;
  return result;
}

} // namespace

class DesktopControlsValuesTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void systemStatusProjectionSkipsUngrantedAndAbsentLanes();
  void commandRankingIsDeterministicAndBounded();
  void commandQueryNormalizationAndMatchKinds();
  void standardPlacesOmitMissingDirectoriesAndKeepComputer();
  void auditedGrantsFollowTheFiveGates();
};

void DesktopControlsValuesTests::systemStatusProjectionSkipsUngrantedAndAbsentLanes()
{
  StatusLaneInput audio{QStringLiteral("audio"), QStringLiteral("Sound"), true, true,
                        QStringLiteral("ready"), QStringLiteral("Output at 50%"),
                        QStringLiteral("audio-volume-medium"), QStringLiteral("Sound 50%"),
                        {}, false};
  StatusLaneInput bluetooth{QStringLiteral("bluetooth"), QStringLiteral("Bluetooth"), true,
                            true, QStringLiteral("unavailable"), {}, {}, {},
                            QStringLiteral("owner lost"), false};
  StatusLaneInput power{QStringLiteral("power"), QStringLiteral("Power"), true, true,
                        QStringLiteral("degraded"), QStringLiteral("12%"),
                        QStringLiteral("battery-020"), {}, {}, true};
  StatusLaneInput ungranted = audio;
  ungranted.id = QStringLiteral("x");
  ungranted.granted = false;
  StatusLaneInput absent = audio;
  absent.id = QStringLiteral("y");
  absent.present = false;

  const SystemStatusModel model =
      projectSystemStatus({audio, bluetooth, power, ungranted, absent});
  QCOMPARE(model.lanes.size(), 3);
  QCOMPARE(model.availableLaneCount, 2);
  QCOMPARE(model.lanes.at(0).available, true);
  QCOMPARE(model.lanes.at(1).available, false);
  QCOMPARE(model.lanes.at(2).available, true);
  QCOMPARE(model.lanes.at(2).attention, true);
  QCOMPARE(model.lanes.at(2).accessibleName, QStringLiteral("Power"));
  QVERIFY(model.accessibleName.contains(QStringLiteral("Output at 50%")));
  QVERIFY(model.accessibleName.contains(QStringLiteral("12%")));
  QVERIFY(model.accessibleDescription.contains(QStringLiteral("Bluetooth")));

  const SystemStatusModel empty = projectSystemStatus({ungranted, absent});
  QVERIFY(empty.lanes.isEmpty());
  QCOMPARE(empty.availableLaneCount, 0);
  QCOMPARE(empty.accessibleName, QStringLiteral("System status"));
}

void DesktopControlsValuesTests::commandRankingIsDeterministicAndBounded()
{
  const QList<CommandCandidate> candidates{
      candidate(QStringLiteral("Text Editor"), QStringLiteral("Application")),
      candidate(QStringLiteral("Terminal"), QStringLiteral("Application")),
      candidate(QStringLiteral("Open Terminal Here"), QStringLiteral("File")),
      candidate(QStringLiteral("Settings"), QStringLiteral("Terminal preferences")),
      candidate(QStringLiteral("Unrelated"), QStringLiteral("Nothing")),
  };
  const QList<CommandCandidate> ranked = rankCommands(QStringLiteral("  term  "), candidates);
  QCOMPARE(ranked.size(), 3);
  QCOMPARE(ranked.at(0).text, QStringLiteral("Terminal"));            // prefix
  QCOMPARE(ranked.at(1).text, QStringLiteral("Open Terminal Here"));  // word start
  QCOMPARE(ranked.at(2).text, QStringLiteral("Settings"));            // detail

  const QList<CommandCandidate> browse = rankCommands(QString{}, candidates, 2);
  QCOMPARE(browse.size(), 2);
  QCOMPARE(browse.at(0).text, QStringLiteral("Text Editor"));
  QCOMPARE(browse.at(1).text, QStringLiteral("Terminal"));

  QList<CommandCandidate> many;
  for (int index = 0; index < 200; ++index) {
    many.append(candidate(QStringLiteral("Item %1").arg(index)));
  }
  QCOMPARE(rankCommands(QStringLiteral("item"), many).size(),
           CommandSearchBounds::maxResults);
  QCOMPARE(rankCommands(QStringLiteral("item"), many, -5).size(), 0);
}

void DesktopControlsValuesTests::commandQueryNormalizationAndMatchKinds()
{
  QCOMPARE(normalizeCommandQuery(QStringLiteral("  a   b  ")), QStringLiteral("a b"));
  QCOMPARE(normalizeCommandQuery(QString(400, QLatin1Char('q'))).size(),
           CommandSearchBounds::maxQueryLength);
  const CommandCandidate editor = candidate(QStringLiteral("Text Editor"),
                                            QStringLiteral("Utilities"));
  QCOMPARE(matchCommand(QStringLiteral("text"), editor), CommandMatch::TextPrefix);
  QCOMPARE(matchCommand(QStringLiteral("edit"), editor), CommandMatch::WordStart);
  QCOMPARE(matchCommand(QStringLiteral("xt ed"), editor), CommandMatch::TextSubstring);
  QCOMPARE(matchCommand(QStringLiteral("util"), editor), CommandMatch::DetailSubstring);
  QCOMPARE(matchCommand(QStringLiteral("zzz"), editor), CommandMatch::None);
  QCOMPARE(matchCommand(QString{}, editor), CommandMatch::TextSubstring);
  QCOMPARE(commandSourceKindText(CommandSourceKind::MenuActions), QStringLiteral("menuAction"));
  QCOMPARE(commandSourceKindText(CommandSourceKind::Workspaces), QStringLiteral("workspace"));
}

void DesktopControlsValuesTests::standardPlacesOmitMissingDirectoriesAndKeepComputer()
{
  const QList<PlaceEntry> none = standardPlaces([](const QString &) { return false; });
  QVERIFY(none.isEmpty());

  const QList<PlaceEntry> all = standardPlaces([](const QString &) { return true; });
  QVERIFY(!all.isEmpty());
  QCOMPARE(all.constFirst().id, QStringLiteral("home"));
  QCOMPARE(all.constLast().id, QStringLiteral("computer"));
  QCOMPARE(all.constLast().path, QDir::rootPath());
  QSet<QString> paths;
  for (const PlaceEntry &entry : all) {
    QVERIFY(QFileInfo(entry.path).isAbsolute());
    QVERIFY(!entry.iconName.isEmpty());
    QVERIFY2(!paths.contains(entry.path), qPrintable(entry.path));
    paths.insert(entry.path);
  }
}

void DesktopControlsValuesTests::auditedGrantsFollowTheFiveGates()
{
  Applets::ManifestCatalog catalog;
  QString error;
  QVERIFY2(catalog.loadDirectory(QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets"), &error),
           qPrintable(error));
  const auto loaded = AppletHost::CapabilityPolicyLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/applet-policy/default.json"));
  QVERIFY2(loaded.ok, qPrintable(loaded.error));
  const auto registry = AppletRuntime::BuiltinAppletRegistry::firstParty();

  const AuditedGrants switcher = evaluateAuditedGrants(catalog, loaded.policy, registry,
                                                       QStringLiteral("workspace-switcher"));
  QVERIFY(switcher.resolved);
  QVERIFY(switcher.has(Applets::Capability::WindowRead));
  QVERIFY(switcher.has(Applets::Capability::WindowManage));
  QVERIFY(!switcher.has(Applets::Capability::WindowActivate));

  const AuditedGrants hud = evaluateAuditedGrants(catalog, loaded.policy, registry,
                                                  QStringLiteral("command-hud"));
  QVERIFY(hud.resolved);
  QCOMPARE(hud.granted, QVector<Applets::Capability>{Applets::Capability::GlobalMenuRead});

  const AuditedGrants missing = evaluateAuditedGrants(catalog, loaded.policy, registry,
                                                      QStringLiteral("weather-forecast"));
  QVERIFY(!missing.resolved);
  QCOMPARE(missing.diagnostic, QStringLiteral("missing-manifest"));
  QVERIFY(!missing.has(Applets::Capability::WindowRead));

  const AppletRuntime::BuiltinAppletRegistry empty(QStringList{});
  const AuditedGrants unregistered = evaluateAuditedGrants(catalog, loaded.policy, empty,
                                                           QStringLiteral("system-menu"));
  QVERIFY(!unregistered.resolved);
  QCOMPARE(unregistered.diagnostic, QStringLiteral("implementation-unavailable"));
}

QTEST_GUILESS_MAIN(DesktopControlsValuesTests)
#include "tst_desktop_controls_values.moc"
