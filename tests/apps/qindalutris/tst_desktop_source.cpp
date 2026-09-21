// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QTest>

#include "desktop_source.h"

using namespace QindaQt::QindaLutris;

// Synthetic XDG data roots with hand-written .desktop entries (ADR-0231).
class tst_desktop_source : public QObject {
  Q_OBJECT

  static void writeEntry(const QString &root, const QString &name,
                         const QByteArray &text) {
    const QString path =
        root + QStringLiteral("/applications/") + name + QStringLiteral(".desktop");
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(text), qint64(text.size()));
  }

private Q_SLOTS:
  void onlyGameCategoriesBecomeGames() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    writeEntry(home.path(), QStringLiteral("tuxracer"),
               "[Desktop Entry]\nType=Application\nName=Tux Racer\n"
               "Exec=tuxracer\nIcon=tuxracer\nCategories=Game;SportsGame;\n");
    writeEntry(home.path(), QStringLiteral("editor"),
               "[Desktop Entry]\nType=Application\nName=An Editor\n"
               "Exec=editor\nCategories=Utility;\n");
    writeEntry(home.path(), QStringLiteral("org.qindaqt.QindaLutris"),
               "[Desktop Entry]\nType=Application\nName=QindaLutris\n"
               "Exec=qindalutris\nCategories=Game;Utility;\n");

    const DesktopDiscovery out = scanDesktopGames({home.path()});
    QCOMPARE(out.games.size(), 1);
    QCOMPARE(out.games.at(0).title, QStringLiteral("Tux Racer"));
    QCOMPARE(out.games.at(0).id, QStringLiteral("desktop/tuxracer"));
    QCOMPARE(out.games.at(0).iconName, QStringLiteral("tuxracer"));
    QVERIFY(out.documentTextById.contains(QStringLiteral("tuxracer")));
  }

  void noRootsNoGames() {
    const DesktopDiscovery out =
        scanDesktopGames({QStringLiteral("/nonexistent-root")});
    QVERIFY(out.games.isEmpty());
  }

  void launchPlanUsesValidatedArgv() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    writeEntry(home.path(), QStringLiteral("tuxracer"),
               "[Desktop Entry]\nType=Application\nName=Tux Racer\n"
               "Exec=tuxracer --fullscreen %f\nIcon=tuxracer\n"
               "Categories=Game;\n");
    const DesktopDiscovery out = scanDesktopGames({home.path()});
    QCOMPARE(out.games.size(), 1);
    const DesktopLaunchPlan plan =
        planDesktopGameLaunch(out, out.games.at(0));
    QVERIFY2(plan.ok, qPrintable(plan.reason));
    QCOMPARE(plan.program, QStringLiteral("tuxracer"));
    QCOMPARE(plan.arguments, QStringList({QStringLiteral("--fullscreen")}));
  }

  void terminalEntryIsNotLaunchable() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    writeEntry(home.path(), QStringLiteral("termgame"),
               "[Desktop Entry]\nType=Application\nName=Term Game\n"
               "Exec=termgame\nTerminal=true\nCategories=Game;\n");
    const DesktopDiscovery out = scanDesktopGames({home.path()});
    QCOMPARE(out.games.size(), 1);
    const DesktopLaunchPlan plan =
        planDesktopGameLaunch(out, out.games.at(0));
    QVERIFY(!plan.ok);
    QVERIFY(!plan.reason.isEmpty());
  }
};

QTEST_GUILESS_MAIN(tst_desktop_source)
#include "tst_desktop_source.moc"
