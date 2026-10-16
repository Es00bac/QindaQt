// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

#include <optional>

namespace QindaQt::Apps::SettingsCenter {

// What the command line asked for. Kept as one value so main() stays a
// composition root rather than an argument parser.
struct LaunchArguments {
  QString page;
  QString destination;
  QString selection;
  QString themeDirectory;
  bool listRoutes = false;
  bool routeConstructionProbe = false;
};

// The outcome of reading the command line. A command line that is fully
// handled without starting Settings (`--help`, or a usage error) comes back as
// an exit status plus the text to print, instead of QCommandLineParser::
// process() calling exit(): that exit ran while the QApplication, and on the
// Settings path everything built after it, was still alive (stability round
// 2026-10-16, findings item 15). main() prints the text and returns, so the
// application object is destroyed the ordinary way.
struct LaunchParseResult {
  LaunchArguments arguments;
  // Set when Settings must not start: 0 for --help, 1 for a usage error,
  // the same statuses QCommandLineParser::process() used.
  std::optional<int> exitStatus;
  QString standardOutput;
  QString standardError;
};

// AGENT-CONTRACT: `--destination` names a tab within the page and `--select`
// an item to select there, for example
//   qindaqt-settings --page input --destination tablet --select <group>
// Both are opaque here: the route page decides whether it knows them, so a
// stale selection opens the route rather than failing to open anything. They
// are bounded so a hostile argument cannot become an unbounded property.
// `commandLine` is the full argument vector, program name first, as
// QCoreApplication::arguments() returns it.
[[nodiscard]] LaunchParseResult parseLaunchArguments(const QStringList &commandLine);

// Writes the outcome's text to stdout and stderr and returns its exit status,
// for main() to return. Only meaningful when `result.exitStatus` is set.
[[nodiscard]] int emitLaunchExit(const LaunchParseResult &result);

} // namespace QindaQt::Apps::SettingsCenter
