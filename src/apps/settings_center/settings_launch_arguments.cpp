// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings_launch_arguments.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>

#include <cstdio>

namespace QindaQt::Apps::SettingsCenter {

LaunchParseResult parseLaunchArguments(const QStringList &commandLine) {
  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("QindaQt Settings"));
  const QCommandLineOption helpOption = parser.addHelpOption();
  const QCommandLineOption pageOption(
      QStringLiteral("page"), QStringLiteral("Open a settings page"),
      QStringLiteral("route"), QStringLiteral("notifications"));
  parser.addOption(pageOption);
  const QCommandLineOption themeDirectoryOption(
      QStringLiteral("theme-directory"),
      QStringLiteral("Additional local theme directory"),
      QStringLiteral("path"));
  parser.addOption(themeDirectoryOption);
  const QCommandLineOption destinationOption(
      QStringLiteral("destination"),
      QStringLiteral("Open a destination within the page"),
      QStringLiteral("destination"));
  parser.addOption(destinationOption);
  const QCommandLineOption selectOption(
      QStringLiteral("select"),
      QStringLiteral("Select this device or item within the destination"),
      QStringLiteral("id"));
  parser.addOption(selectOption);
  const QCommandLineOption listRoutesOption(
      QStringLiteral("list-routes"),
      QStringLiteral("List registered route IDs for package verification"));
  const QCommandLineOption routeProbeOption(
      QStringLiteral("route-construction-probe"),
      QStringLiteral("Exit after the requested route Loader resolves"));
  parser.addOption(listRoutesOption);
  parser.addOption(routeProbeOption);

  LaunchParseResult result;
  // AGENT-GUARD: parse(), never process(). process() calls exit() for --help
  // and for a usage error, which terminates the process with the QApplication
  // alive; every early exit here is a return from main() instead.
  if (!parser.parse(commandLine)) {
    result.exitStatus = 1;
    result.standardError = QCoreApplication::applicationName()
        + QStringLiteral(": ") + parser.errorText() + QLatin1Char('\n');
    return result;
  }
  if (parser.isSet(helpOption)) {
    result.exitStatus = 0;
    result.standardOutput = parser.helpText();
    return result;
  }
  result.arguments = LaunchArguments{
      parser.value(pageOption),
      parser.value(destinationOption).left(64),
      parser.value(selectOption).left(256),
      parser.value(themeDirectoryOption),
      parser.isSet(listRoutesOption),
      parser.isSet(routeProbeOption),
  };
  return result;
}

int emitLaunchExit(const LaunchParseResult &result) {
  if (!result.standardOutput.isEmpty()) {
    std::fputs(qPrintable(result.standardOutput), stdout);
  }
  if (!result.standardError.isEmpty()) {
    std::fputs(qPrintable(result.standardError), stderr);
  }
  return result.exitStatus.value_or(0);
}

} // namespace QindaQt::Apps::SettingsCenter
