// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QTest>

#include "library_controller.h"

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
    controller->setSteamRootsForProton({steam});
    controller->setLutrisDatabasePath(root + QStringLiteral("/no-pga.db"));
    writeFile(root + QStringLiteral("/xdg/applications/tux.desktop"),
              "[Desktop Entry]\nType=Application\nName=Tux Fixture\n"
              "Exec=tux --play %f\nIcon=tux\nCategories=Game;\n");
    controller->setDesktopDataRoots({root + QStringLiteral("/xdg")});
    controller->setConfigRoot(root + QStringLiteral("/config"));
    controller->setCoverCacheDir(root + QStringLiteral("/cache"));
    controller->setProcessLauncher(launcher);
    controller->refresh();
    return controller;
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
};

} // namespace

QTEST_MAIN(tst_library_controller)
#include "tst_library_controller.moc"
