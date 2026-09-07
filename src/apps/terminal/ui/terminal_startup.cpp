// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_startup.h"

#include "profiles/terminal_profile_settings.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QProcess>

#include <algorithm>

namespace QindaQt::Apps::Terminal {

std::function<QString(const TerminalProfile &, const QString &)>
makeNewTerminalLauncher(const QCommandLineParser &parser) {
  const QString executable = QCoreApplication::applicationFilePath();
  const bool hasTheme = parser.isSet(QStringLiteral("theme"));
  const QString theme = parser.value(QStringLiteral("theme"));
  const bool hasThemeDirectory = parser.isSet(QStringLiteral("theme-directory"));
  const QString themeDirectory = parser.value(QStringLiteral("theme-directory"));
  const bool hasShell = parser.isSet(QStringLiteral("shell"));
  const QString shell = parser.value(QStringLiteral("shell"));
  const QStringList shellArguments = parser.values(QStringLiteral("arg"));

  return [executable, hasTheme, theme, hasThemeDirectory, themeDirectory,
          hasShell, shell, shellArguments](const TerminalProfile &profile,
                                           const QString &workingDirectory) {
    QStringList arguments;
    arguments << QStringLiteral("--profile") << profile.id;
    if (hasTheme) {
      arguments << QStringLiteral("--theme") << theme;
    }
    if (hasThemeDirectory) {
      arguments << QStringLiteral("--theme-directory") << themeDirectory;
    }
    if (hasShell) {
      arguments << QStringLiteral("--shell") << shell;
    }
    for (const QString &argument : shellArguments) {
      arguments << QStringLiteral("--arg") << argument;
    }
    if (!workingDirectory.isEmpty()) {
      arguments << QStringLiteral("--working-directory") << workingDirectory;
    }
    if (!QProcess::startDetached(executable, arguments)) {
      return QStringLiteral("could not start another Terminal process");
    }
    return QString();
  };
}

bool initialSessionReady(const TerminalProfileSettings &settings,
                         const bool definitiveFallback) {
  return settings.baselineReceived() || definitiveFallback;
}

TerminalProfile initialSessionProfile(
    const TerminalProfileSettings &settings, const QString &requestedProfileId,
    QString *unavailableProfileDiagnostic) {
  if (unavailableProfileDiagnostic != nullptr) {
    unavailableProfileDiagnostic->clear();
  }
  if (requestedProfileId == builtinDefaultProfileId()) {
    return builtinDefaultProfile();
  }

  TerminalProfile profile = settings.defaultProfile();
  if (requestedProfileId.isEmpty()) {
    return profile;
  }
  const auto profiles = settings.userProfiles();
  const auto found = std::find_if(
      profiles.cbegin(), profiles.cend(), [&requestedProfileId](
                                           const TerminalProfile &candidate) {
        return candidate.id == requestedProfileId;
      });
  if (found != profiles.cend()) {
    return *found;
  }
  if (unavailableProfileDiagnostic != nullptr) {
    *unavailableProfileDiagnostic = requestedProfileId;
  }
  return profile;
}

} // namespace QindaQt::Apps::Terminal
