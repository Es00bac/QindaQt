// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings_launch_arguments.h"

#include <QCoreApplication>
#include <QtTest>

using namespace QindaQt::Apps::SettingsCenter;

// AGENT-CONTRACT (ADR-0395): command-line handling never calls exit(). --help
// and usage errors come back as a status and the text to print, so main()
// returns and the QApplication is destroyed the ordinary way. These rows pin
// that the parser reports those outcomes as values instead of terminating the
// test process (a process::exit here would end this very executable).
class SettingsLaunchArgumentsTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void helpIsAStatusAndTextNotAnExit_data();
  void helpIsAStatusAndTextNotAnExit();
  void usageErrorsAreAStatusAndMessageNotAnExit_data();
  void usageErrorsAreAStatusAndMessageNotAnExit();
  void ordinaryArgumentsParseWithDefaultsAndBounds();
  void probeAndListFlagsAreCarried();
};

void SettingsLaunchArgumentsTests::helpIsAStatusAndTextNotAnExit_data() {
  QTest::addColumn<QStringList>("commandLine");
  QTest::newRow("long") << QStringList{QStringLiteral("qindaqt-settings"), QStringLiteral("--help")};
  QTest::newRow("short") << QStringList{QStringLiteral("qindaqt-settings"), QStringLiteral("-h")};
  // Help wins over everything else on the line, as with process().
  QTest::newRow("with-page") << QStringList{QStringLiteral("qindaqt-settings"),
                                            QStringLiteral("--page"), QStringLiteral("audio"),
                                            QStringLiteral("--help")};
}

void SettingsLaunchArgumentsTests::helpIsAStatusAndTextNotAnExit() {
  QFETCH(QStringList, commandLine);
  const LaunchParseResult result = parseLaunchArguments(commandLine);
  QVERIFY(result.exitStatus.has_value());
  QCOMPARE(*result.exitStatus, 0);
  QVERIFY(result.standardOutput.contains(QStringLiteral("Usage:")));
  QVERIFY(result.standardOutput.contains(QStringLiteral("--page")));
  QVERIFY(result.standardOutput.contains(QStringLiteral("--route-construction-probe")));
  QVERIFY(result.standardError.isEmpty());
}

void SettingsLaunchArgumentsTests::usageErrorsAreAStatusAndMessageNotAnExit_data() {
  QTest::addColumn<QStringList>("commandLine");
  QTest::addColumn<QString>("expected");
  QTest::newRow("unknown-option")
      << QStringList{QStringLiteral("qindaqt-settings"), QStringLiteral("--no-such-option")}
      << QStringLiteral("Unknown option");
  QTest::newRow("missing-value")
      << QStringList{QStringLiteral("qindaqt-settings"), QStringLiteral("--page")}
      << QStringLiteral("Missing value");
  QTest::newRow("flag-with-value")
      << QStringList{QStringLiteral("qindaqt-settings"), QStringLiteral("--list-routes=yes")}
      << QStringLiteral("Unexpected value");
}

void SettingsLaunchArgumentsTests::usageErrorsAreAStatusAndMessageNotAnExit() {
  QFETCH(QStringList, commandLine);
  QFETCH(QString, expected);
  const LaunchParseResult result = parseLaunchArguments(commandLine);
  QVERIFY(result.exitStatus.has_value());
  QCOMPARE(*result.exitStatus, 1);
  QVERIFY2(result.standardError.contains(expected), qPrintable(result.standardError));
  QVERIFY(result.standardOutput.isEmpty());
}

void SettingsLaunchArgumentsTests::ordinaryArgumentsParseWithDefaultsAndBounds() {
  const LaunchParseResult defaults =
      parseLaunchArguments({QStringLiteral("qindaqt-settings")});
  QVERIFY(!defaults.exitStatus.has_value());
  QCOMPARE(defaults.arguments.page, QStringLiteral("notifications"));
  QVERIFY(defaults.arguments.destination.isEmpty());
  QVERIFY(defaults.arguments.selection.isEmpty());
  QVERIFY(!defaults.arguments.listRoutes);
  QVERIFY(!defaults.arguments.routeConstructionProbe);

  const LaunchParseResult parsed = parseLaunchArguments(
      {QStringLiteral("qindaqt-settings"), QStringLiteral("--page"), QStringLiteral("input"),
       QStringLiteral("--destination"), QString(200, QLatin1Char('d')),
       QStringLiteral("--select"), QString(900, QLatin1Char('s')),
       QStringLiteral("--theme-directory"), QStringLiteral("/tmp/themes")});
  QVERIFY(!parsed.exitStatus.has_value());
  QCOMPARE(parsed.arguments.page, QStringLiteral("input"));
  // A hostile argument cannot become an unbounded property.
  QCOMPARE(parsed.arguments.destination.size(), 64);
  QCOMPARE(parsed.arguments.selection.size(), 256);
  QCOMPARE(parsed.arguments.themeDirectory, QStringLiteral("/tmp/themes"));
}

void SettingsLaunchArgumentsTests::probeAndListFlagsAreCarried() {
  const LaunchParseResult probe = parseLaunchArguments(
      {QStringLiteral("qindaqt-settings"), QStringLiteral("--page"), QStringLiteral("audio"),
       QStringLiteral("--route-construction-probe")});
  QVERIFY(!probe.exitStatus.has_value());
  QVERIFY(probe.arguments.routeConstructionProbe);
  QVERIFY(!probe.arguments.listRoutes);
  const LaunchParseResult list =
      parseLaunchArguments({QStringLiteral("qindaqt-settings"), QStringLiteral("--list-routes")});
  QVERIFY(!list.exitStatus.has_value());
  QVERIFY(list.arguments.listRoutes);
}

QTEST_GUILESS_MAIN(SettingsLaunchArgumentsTests)
#include "tst_settings_launch_arguments.moc"
