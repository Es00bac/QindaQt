// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/process_liveness.h"
#include "session/terminal_launch_policy.h"
#include "session/terminal_session.h"
#include "ui/terminal_appearance.h"
#include "ui/terminal_widget_adapter.h"
#include "ui/terminal_window.h"

#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/themes/theme_loader.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>

#include <cstdio>
#include <memory>

namespace QindaQt::Apps::Terminal {
namespace {

// AGENT-NOTE: Duplicated from the Text Editor S1 launcher on purpose: the
// shared application-shell seam (QQ-006.03) does not exist on this base yet,
// and creating a second framework would repeat the problem it would solve.
// When that seam lands, both launchers should consume it.
[[nodiscard]] QStringList
themeSearchDirectories(const QString &explicitDirectory) {
  QStringList directories;
  if (!explicitDirectory.isEmpty()) {
    directories.append(QFileInfo(explicitDirectory).absoluteFilePath());
  }
  directories.append(QStandardPaths::locateAll(
      QStandardPaths::GenericDataLocation, QStringLiteral("qindaqt/themes"),
      QStandardPaths::LocateDirectory));
  directories.append(
      QDir(QCoreApplication::applicationDirPath())
          .absoluteFilePath(QStringLiteral("../share/qindaqt/themes")));
  directories.removeDuplicates();
  return directories;
}

[[nodiscard]] QindaQt::Themes::LoadResult
loadTheme(const QString &themeId, const QStringList &directories) {
  static const QRegularExpression safeId(
      QStringLiteral("^[a-z0-9][a-z0-9-]{0,63}$"));
  if (!safeId.match(themeId).hasMatch()) {
    return {.ok = false,
            .theme = {},
            .error = QStringLiteral("Invalid theme identifier")};
  }
  for (const QString &directory : directories) {
    const QString path =
        QDir(directory).filePath(themeId + QStringLiteral(".json"));
    if (QFileInfo::exists(path)) {
      return QindaQt::Themes::ThemeLoader::fromFile(path);
    }
  }
  return {.ok = false,
          .theme = {},
          .error = QStringLiteral("Theme '%1' was not found").arg(themeId)};
}

} // namespace
} // namespace QindaQt::Apps::Terminal

int main(int argc, char **argv) {
  using namespace QindaQt::Apps::Terminal;

  QApplication application(argc, argv);
  application.setApplicationName(QStringLiteral("qindaqt-terminal"));
  application.setApplicationDisplayName(QStringLiteral("QindaQt Terminal"));
  application.setOrganizationName(QStringLiteral("QindaQt"));
  application.setDesktopFileName(QStringLiteral("org.qindaqt.Terminal"));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("QindaQt terminal for the configured shell"));
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addOption({QStringLiteral("theme"),
                    QStringLiteral("QindaQt theme identifier"),
                    QStringLiteral("id"), QStringLiteral("qinda-dark")});
  parser.addOption({QStringLiteral("theme-directory"),
                    QStringLiteral("Additional local theme directory"),
                    QStringLiteral("path")});
  parser.addOption({QStringLiteral("shell"),
                    QStringLiteral("Absolute shell executable path"),
                    QStringLiteral("program")});
  parser.addOption({QStringLiteral("working-directory"),
                    QStringLiteral("Initial working directory"),
                    QStringLiteral("directory")});
  parser.addOption(
      {QStringLiteral("check-theme"),
       QStringLiteral("Validate the selected theme through QST-1 and exit")});
  parser.addOption({QStringLiteral("arg"),
                    QStringLiteral("Argument passed verbatim to the shell "
                                   "(repeatable, never shell-interpreted)"),
                    QStringLiteral("value")});
  parser.process(application);
  // Positional arguments are rejected so no caller can mistake this CLI for
  // shell-string semantics: a command is always argv here.
  if (!parser.positionalArguments().isEmpty()) {
    std::fprintf(stderr,
                 "qindaqt-terminal: unexpected positional arguments; use "
                 "--arg for shell arguments\n");
    return 2;
  }

  const auto theme = loadTheme(
      parser.value(QStringLiteral("theme")),
      themeSearchDirectories(parser.value(QStringLiteral("theme-directory"))));
  if (!theme.ok) {
    std::fprintf(stderr, "qindaqt-terminal: %s\n", qPrintable(theme.error));
    return 3;
  }
  const auto appearance =
      TerminalAppearanceAdapter::fromTheme(theme.theme);
  if (!appearance.ok()) {
    std::fprintf(stderr, "qindaqt-terminal: %s\n",
                 qPrintable(appearance.diagnostic));
    return 3;
  }
  application.setPalette(appearance.appearance->windowPalette);
  application.setFont(appearance.appearance->interfaceFont);
  if (parser.isSet(QStringLiteral("check-theme"))) {
    std::printf("%s qst-%d\n",
                qPrintable(appearance.appearance->sourceThemeId),
                QindaQt::DesignTokens::DesignTokens::qstRevision);
    return 0;
  }

  const QStringList baseEnvironment = QProcessEnvironment::systemEnvironment()
                                          .toStringList();
  const auto environment =
      TerminalLaunchPolicy::childEnvironment(baseEnvironment);
  if (!environment.outcome.ok) {
    std::fprintf(stderr, "qindaqt-terminal: %s\n",
                 qPrintable(environment.outcome.diagnostic));
    return 4;
  }
  const auto resolution = TerminalLaunchPolicy::resolveShell(
      parser.value(QStringLiteral("shell")), parser.values(QStringLiteral("arg")),
      parser.value(QStringLiteral("working-directory")), baseEnvironment);
  if (!resolution.outcome.ok) {
    std::fprintf(stderr, "qindaqt-terminal: %s\n",
                 qPrintable(resolution.outcome.diagnostic));
    return 4;
  }
  auto request = resolution.request;
  request.environment = environment.environment;

  PosixProcessMonitor monitor;
  TerminalSession::BackendFactory factory = [&appearance] {
    return std::unique_ptr<TerminalSessionBackend>(
        new TerminalWidgetAdapter(*appearance.appearance));
  };
  auto session = std::make_unique<TerminalSession>(
      std::move(factory), &monitor, TeardownBounds{}, nullptr);

  TerminalWindow window(std::move(session), *appearance.appearance);
  window.resize(800, 500);
  if (!window.session()->start(request)) {
    // The window stays available so the typed failure and Restart action are
    // user-visible; a start failure is not a launcher failure.
    std::fprintf(stderr, "qindaqt-terminal: %s\n",
                 qPrintable(window.session()->lastExit().diagnostic));
  }
  window.show();

  QObject::connect(
      &window, &TerminalWindow::closeShutdownFinished, &application,
      &QApplication::quit, Qt::QueuedConnection);
  return application.exec();
}
