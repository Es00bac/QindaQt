// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QTest>

#include "library_controller.h"
#include "title_store.h"

using namespace QindaQt::QindaLutris;

namespace {

// The ADR-0062-style recording fake: proves what the controller would have
// launched without starting anything.
class RecordingLauncher final : public GameProcessLauncher {
public:
  QVector<LaunchPlan> plans;
  LaunchOutcome launch(const LaunchPlan &plan) override {
    plans.append(plan);
    return {true, {}};
  }
};

void writeFile(const QString &path, const QByteArray &bytes) {
  QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  QCOMPARE(file.write(bytes), qint64(bytes.size()));
}

class tst_library_controller : public QObject {
  Q_OBJECT

  // A controller over one synthetic machine: a Steam manifest, a Lutris row,
  // a desktop game, an empty config root, and the recording launcher.
  static LibraryController *makeController(const QString &root,
                                           RecordingLauncher *launcher,
                                           QObject *parent) {
    auto *controller = new LibraryController(parent);
    const QString steam = root + QStringLiteral("/steam");
    writeFile(steam + QStringLiteral("/steamapps/appmanifest_9.acf"),
              "\"AppState\"\n{\n\t\"name\"\t\t\"Fixture Steam Game\"\n}\n");
    controller->setSteamCandidates({steam});
    // One fixture Proton build and a fixture umu-run (ADR-0275): the suite
    // never sees the host's /usr/share/steam/compatibilitytools.d or umu.
    const QString protonRoot = root + QStringLiteral("/compat");
    writeFile(protonRoot + QStringLiteral("/GE-Proton11-6/proton"),
              "#!/bin/sh\nexit 0\n");
    QFile::setPermissions(protonRoot + QStringLiteral("/GE-Proton11-6/proton"),
                          QFile::ReadOwner | QFile::WriteOwner
                              | QFile::ExeOwner);
    controller->setProtonRoots({{protonRoot, ProtonBuild::Origin::System}});
    controller->setLutrisDatabasePath(root + QStringLiteral("/no-pga.db"));
    writeFile(root + QStringLiteral("/xdg/applications/tux.desktop"),
              "[Desktop Entry]\nType=Application\nName=Tux Fixture\n"
              "Exec=tux --play %f\nIcon=tux\nCategories=Game;\n");
    controller->setDesktopDataRoots({root + QStringLiteral("/xdg")});
    controller->setConfigRoot(root + QStringLiteral("/config"));
    controller->setCoverCacheDir(root + QStringLiteral("/cache"));
    controller->setProcessLauncher(launcher);
    // A fixture Wine loader, so the suite does not depend on the host having
    // one. This test used to fail on any machine without a plain `wine` on
    // PATH - which includes every Gentoo box running wine-proton, since that
    // package installs only versioned loaders.
    const QString bin = root + QStringLiteral("/bin");
    writeFile(bin + QStringLiteral("/wine"), "#!/bin/sh\nexit 0\n");
    QFile::setPermissions(bin + QStringLiteral("/wine"),
                          QFile::ReadOwner | QFile::WriteOwner
                              | QFile::ExeOwner);
    controller->setWineLoaderSearchPath({bin});
    writeFile(bin + QStringLiteral("/umu-run"), "#!/bin/sh\nexit 0\n");
    QFile::setPermissions(bin + QStringLiteral("/umu-run"),
                          QFile::ReadOwner | QFile::WriteOwner
                              | QFile::ExeOwner);
    controller->setUmuSearchPath({bin});
    controller->refresh();
    return controller;
  }

  static QString buildPath(const QString &root) {
    return QDir(root + QStringLiteral("/compat/GE-Proton11-6")).canonicalPath();
  }

private Q_SLOTS:
  void refreshMergesSources() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    RecordingLauncher launcher;
    QScopedPointer<LibraryController> controller(
        makeController(home.path(), &launcher, nullptr));
    QCOMPARE(controller->totalCount(), 2);
    QVERIFY(controller->sourcesPresent().contains(QStringLiteral("steam")));
    QVERIFY(controller->sourcesPresent().contains(QStringLiteral("desktop")));
  }

  void addWineGamePersistsAcrossInstances() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    RecordingLauncher launcher;
    QScopedPointer<LibraryController> controller(
        makeController(home.path(), &launcher, nullptr));
    QVERIFY(controller->addWineGame(
        QStringLiteral("Alpha"), home.path() + QStringLiteral("/alpha/game.exe"),
        home.path() + QStringLiteral("/alpha/prefix"),
        QStringLiteral("wine"), QString()));
    QCOMPARE(controller->totalCount(), 3);
    QVERIFY(controller->selectedGameId().startsWith(QStringLiteral("wine/")));

    // A second instance over the same config root sees the same library.
    QScopedPointer<LibraryController> again(
        makeController(home.path(), &launcher, nullptr));
    QCOMPARE(again->totalCount(), 3);
    QVERIFY(again->sourcesPresent().contains(QStringLiteral("wine")));

    // A duplicate executable is refused rather than added twice.
    QVERIFY(!again->addWineGame(
        QStringLiteral("Alpha"), home.path() + QStringLiteral("/alpha/game.exe"),
        QString(), QStringLiteral("wine"), QString()));
  }

  void optionsPersistAndPlayUsesThem() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    RecordingLauncher launcher;
    QScopedPointer<LibraryController> controller(
        makeController(home.path(), &launcher, nullptr));
    QVERIFY(controller->addWineGame(
        QStringLiteral("Alpha"), home.path() + QStringLiteral("/alpha/game.exe"),
        home.path() + QStringLiteral("/alpha/prefix"),
        QStringLiteral("wine"), QString()));

    QVariantMap values;
    values.insert(QStringLiteral("gamemode"), true);
    values.insert(QStringLiteral("mangohud"), true);
    values.insert(QStringLiteral("display"), QString());
    values.insert(QStringLiteral("environment"),
                  QStringLiteral("DXVK_HUD=compiler\nNOT VALID\nMANGOHUD_CONFIG=fps"));
    values.insert(QStringLiteral("runner"), QStringLiteral("wine"));
    values.insert(QStringLiteral("prefixOverride"), QString());
    controller->saveLaunchOptionsForSelected(values);

    const QVariantMap stored = controller->launchOptionsForSelected();
    QVERIFY(stored.value(QStringLiteral("gamemode")).toBool());
    QCOMPARE(stored.value(QStringLiteral("environment")).toString(),
             QStringLiteral("DXVK_HUD=compiler\nMANGOHUD_CONFIG=fps"));

    // A second instance reads the same options back.
    QScopedPointer<LibraryController> again(
        makeController(home.path(), &launcher, nullptr));
    again->selectGame(controller->selectedGameId());
    QCOMPARE(again->launchOptionsForSelected(), stored);

    // Play reaches the (fake) spawner as pure argv, never a shell string.
    launcher.plans.clear();
    again->playSelected();
    QCOMPARE(launcher.plans.size(), 1);
    const LaunchPlan &plan = launcher.plans.constFirst();
    QVERIFY(plan.ok);
    QVERIFY(!plan.program.isEmpty());
    QCOMPARE(plan.environment.value(QStringLiteral("DXVK_HUD")),
             QStringLiteral("compiler"));
    QCOMPARE(plan.environment.value(QStringLiteral("MANGOHUD")),
             QStringLiteral("1"));
  }

  void removeWineGamePersists() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    RecordingLauncher launcher;
    QScopedPointer<LibraryController> controller(
        makeController(home.path(), &launcher, nullptr));
    QVERIFY(controller->addWineGame(
        QStringLiteral("Alpha"), home.path() + QStringLiteral("/alpha/game.exe"),
        QString(), QStringLiteral("wine"), QString()));
    QCOMPARE(controller->totalCount(), 3);
    controller->removeWineGame(controller->selectedGameId());
    QCOMPARE(controller->totalCount(), 2);
    QScopedPointer<LibraryController> again(
        makeController(home.path(), &launcher, nullptr));
    QCOMPARE(again->totalCount(), 2);
  }

  // ADR-0275: a new hand-added entry records the default build by NAME, and
  // a Proton entry then launches through umu with that build as PROTONPATH.
  void addedProtonEntryPinsTheDefaultBuild() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    RecordingLauncher launcher;
    QScopedPointer<LibraryController> controller(
        makeController(home.path(), &launcher, nullptr));
    const QString exe = home.path() + QStringLiteral("/beta/game.exe");
    writeFile(exe, "MZ");
    QVERIFY(controller->addWineGame(
        QStringLiteral("Beta"), exe, home.path() + QStringLiteral("/beta/prefix"),
        QStringLiteral("proton"), QString()));
    QCOMPARE(controller->selectedGame().value(QStringLiteral("protonBuild")),
             QVariant(QStringLiteral("GE-Proton11-6")));
    LibraryStore::Error error = LibraryStore::Error::None;
    const QVector<WineEntryRecord> stored =
        LibraryStore(home.path() + QStringLiteral("/config")).readWineEntries(&error);
    QCOMPARE(stored.size(), 1);
    QCOMPARE(stored.at(0).protonPath, QStringLiteral("GE-Proton11-6"));

    controller->playSelected();
    QCOMPARE(launcher.plans.size(), 1);
    const LaunchPlan &plan = launcher.plans.constFirst();
    QVERIFY(plan.program.endsWith(QStringLiteral("/umu-run")));
    QCOMPARE(plan.environment.value(QStringLiteral("PROTONPATH")),
             buildPath(home.path()));
    QCOMPARE(plan.environment.value(QStringLiteral("UMU_RUNTIME_UPDATE")),
             QStringLiteral("0"));

    // The path protonChoices() offers is recorded as the build name too.
    const QVariantList choices = controller->protonChoices();
    QCOMPARE(choices.size(), 1);
    const QVariantMap choice = choices.constFirst().toMap();
    QCOMPARE(choice.value(QStringLiteral("build")), QVariant(QStringLiteral("GE-Proton11-6")));
    QVERIFY(choice.value(QStringLiteral("isDefault")).toBool());
    QVERIFY(controller->addWineGame(
        QStringLiteral("Gamma"), home.path() + QStringLiteral("/gamma.exe"),
        QString(), QStringLiteral("proton"),
        choice.value(QStringLiteral("path")).toString()));
    QCOMPARE(controller->selectedGame().value(QStringLiteral("protonBuild")),
             QVariant(QStringLiteral("GE-Proton11-6")));

    // A floating alias or an uninstalled build is never recorded.
    QVERIFY(!controller->addWineGame(
        QStringLiteral("Delta"), home.path() + QStringLiteral("/delta.exe"),
        QString(), QStringLiteral("proton"), QStringLiteral("GE-Proton")));
    QVERIFY(!controller->addWineGame(
        QStringLiteral("Delta"), home.path() + QStringLiteral("/delta.exe"),
        QString(), QStringLiteral("proton"), QStringLiteral("GE-Proton11-7")));
  }

  // A title in titles-v1.json is an Installed game that plays through umu.
  void installedTitleAppearsAndPlaysThroughUmu() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString prefix = home.path() + QStringLiteral("/Games/battlenet");
    const QString exe = prefix + QStringLiteral("/drive_c/Battle.net/Battle.net.exe");
    writeFile(exe, "MZ");
    TitleRecord record;
    record.id = QStringLiteral("title/battle-net");
    record.title = QStringLiteral("Battle.net");
    record.kind = TitleKind::StoreLauncher;
    record.store = GameStore::BattleNet;
    record.prefixPath = prefix;
    record.protonBuild = QStringLiteral("GE-Proton11-6");
    record.umuStore = QStringLiteral("battlenet");
    record.executable = exe;
    record.installedAt = QStringLiteral("2026-09-25");
    QCOMPARE(TitleStore(home.path() + QStringLiteral("/config")).writeTitles({record}),
             TitleStore::Error::None);

    RecordingLauncher launcher;
    QScopedPointer<LibraryController> controller(
        makeController(home.path(), &launcher, nullptr));
    QCOMPARE(controller->totalCount(), 3);
    QVERIFY(controller->sourcesPresent().contains(QStringLiteral("installed")));
    controller->selectGame(QStringLiteral("title/battle-net"));
    QCOMPARE(controller->selectedGame().value(QStringLiteral("sourceId")),
             QVariant(QStringLiteral("installed")));
    QVERIFY2(controller->selectedPlayable(),
             qPrintable(controller->selectedPlayReason()));
    controller->playSelected();
    QCOMPARE(launcher.plans.size(), 1);
    const LaunchPlan &plan = launcher.plans.constFirst();
    QVERIFY(plan.program.endsWith(QStringLiteral("/umu-run")));
    QCOMPARE(plan.arguments, QStringList({exe}));
    QCOMPARE(plan.environment.value(QStringLiteral("PROTONPATH")),
             buildPath(home.path()));
    QCOMPARE(plan.environment.value(QStringLiteral("WINEPREFIX")), prefix);
    QCOMPARE(plan.environment.value(QStringLiteral("STORE")),
             QStringLiteral("battlenet"));
  }

  // The pinned build disappears: the title stays listed, Play says why,
  // and nothing is launched with another build.
  void installedTitleWithMissingBuildIsNotPlayable() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString prefix = home.path() + QStringLiteral("/Games/wow");
    writeFile(prefix + QStringLiteral("/Wow.exe"), "MZ");
    TitleRecord record;
    record.id = QStringLiteral("title/wow");
    record.title = QStringLiteral("World of Warcraft");
    record.prefixPath = prefix;
    record.protonBuild = QStringLiteral("GE-Proton10-25");
    record.executable = prefix + QStringLiteral("/Wow.exe");
    record.installedAt = QStringLiteral("2026-09-25");
    QCOMPARE(TitleStore(home.path() + QStringLiteral("/config")).writeTitles({record}),
             TitleStore::Error::None);
    RecordingLauncher launcher;
    QScopedPointer<LibraryController> controller(
        makeController(home.path(), &launcher, nullptr));
    controller->selectGame(QStringLiteral("title/wow"));
    QVERIFY(!controller->selectedPlayable());
    QVERIFY(controller->selectedPlayReason().startsWith(
        QStringLiteral("GE-Proton10-25 is not installed.")));
    controller->playSelected();
    QVERIFY(launcher.plans.isEmpty());
  }
};

} // namespace

QTEST_MAIN(tst_library_controller)
#include "tst_library_controller.moc"
