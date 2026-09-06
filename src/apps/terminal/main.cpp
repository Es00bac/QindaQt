// SPDX-License-Identifier: GPL-3.0-or-later
#include "links/terminal_link_opener.h"
#include "profiles/terminal_profile_settings.h"
#include "session/process_liveness.h"
#include "session/terminal_launch_policy.h"
#include "session/terminal_session.h"
#include "session/terminal_session_collection.h"
#include "ui/terminal_appearance.h"
#include "ui/terminal_widget_adapter.h"
#include "ui/terminal_window.h"

#include "qindaqt/app_appearance/application_appearance_controller.h"
#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/services/font_discovery/font_session_bootstrap.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/themes/theme_loader.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMenuBar>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QWindow>

#include <cstdio>
#include <memory>

#include <qindaqt/app_shell/menu_export/first_party_composition.h>

namespace QindaQt::Apps::Terminal {
namespace {

// AGENT-NOTE: Duplicated from the Text Editor S1 launcher on purpose: the
// shared application-shell seam is owned by a different lane, and this
// launcher must not reach into another module's private code. When that seam
// is part of this branch's integration base, both launchers should consume
// it instead of duplicating the lookup.
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

// Every installed, schema-valid theme id, for the profile dialog's color
// scheme selection. Load failures are skipped: a theme that cannot be
// validated is never offered to a profile.
[[nodiscard]] QStringList availableThemeIds(const QStringList &directories) {
  QStringList ids;
  for (const QString &directory : directories) {
    const QFileInfoList entries = QDir(directory).entryInfoList(
        {QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QFileInfo &entry : entries) {
      const QString id = entry.completeBaseName();
      if (ids.contains(id)) {
        continue;
      }
      if (QindaQt::Themes::ThemeLoader::fromFile(entry.absoluteFilePath()).ok) {
        ids.append(id);
      }
    }
  }
  ids.sort();
  return ids;
}

void configureCommandLine(QCommandLineParser &parser) {
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
}

// AGENT-CONTRACT: first-party global-menu export — the shared AppShell
// composition entry (docs/wiki/shell/global-menu.md). Call exactly once, after
// the window is shown so its platform QWindow exists. A null return is the
// fail-closed outcome: the local QMenuBar stays the only authority.
[[nodiscard]] std::unique_ptr<QObject>
composeTerminalMenuExport(TerminalWindow &window) {
  QWindow *windowHandle = window.windowHandle();
  if (windowHandle == nullptr) {
    return {};
  }
  return QindaQt::AppShell::MenuExport::composeFirstPartyMenuExport(
      window.appShellCoordinator(), *windowHandle,
      QDBusConnection::sessionBus(),
      [&window](bool visible) { window.menuBar()->setVisible(visible); });
}

// The factory resolves the profile's color scheme to its own QST generation
// per session; an unresolvable theme yields a null backend, which the session
// reports as a typed StartFailed exit (fail-closed). Captured by value: the
// factory outlives this call inside TerminalSessionCollection.
[[nodiscard]] TerminalSession::BackendFactory
makeBackendFactory(const QStringList &themeDirectories,
                   const QString &launchThemeId) {
  return [themeDirectories, launchThemeId](const TerminalProfile &profile) {
    // Preserve the S0 --theme contract for the immutable built-in profile.
    // User profiles carry their own Settings1-backed QST theme.
    const QString themeId = profile.id == builtinDefaultProfileId()
                                ? launchThemeId
                                                : profile.colorSchemeId;
    const auto profileTheme = loadTheme(themeId, themeDirectories);
    if (!profileTheme.ok) {
      return std::unique_ptr<TerminalSessionBackend>(nullptr);
    }
    const auto profileAppearance =
        TerminalAppearanceAdapter::fromTheme(profileTheme.theme);
    if (!profileAppearance.ok()) {
      return std::unique_ptr<TerminalSessionBackend>(nullptr);
    }
    return std::unique_ptr<TerminalSessionBackend>(
        new TerminalWidgetAdapter(*profileAppearance.appearance, profile));
  };
}

class TerminalAppearanceBinding final {
public:
  TerminalAppearanceBinding(QApplication &application, TerminalWindow &window,
                            const QStringList &directories,
                            const QString &explicitTheme)
      : transport(QDBusConnection::sessionBus()),
        client(transport, {QStringLiteral("appearance.theme"),
                           QStringLiteral("appearance.colorScheme")}),
        controller(client, directories, QStringLiteral("qinda-dark"),
                   explicitTheme),
        application(application), window(window) {
    QObject::connect(&controller,
                     &QindaQt::AppAppearance::ApplicationAppearanceController::
                         appearanceChanged,
                     &window, [this] { apply(); });
    apply();
    QString error;
    if (!client.start(&error))
      qWarning().noquote()
          << "QindaQt Terminal appearance settings unavailable:" << error;
  }

private:
  void apply() {
    const auto adapted =
        TerminalAppearanceAdapter::fromTheme(controller.theme());
    if (!adapted.ok())
      return;
    application.setPalette(adapted.appearance->windowPalette);
    application.setFont(adapted.appearance->interfaceFont);
    window.applyAppearance(*adapted.appearance);
  }
  QindaQt::Services::SettingsClient::QtSettingsTransport transport;
  QindaQt::Services::SettingsClient::SettingsClient client;
  QindaQt::AppAppearance::ApplicationAppearanceController controller;
  QApplication &application;
  TerminalWindow &window;
};

} // namespace
} // namespace QindaQt::Apps::Terminal

int main(int argc, char **argv) {
  using namespace QindaQt::Apps::Terminal;

  // AGENT-CONTRACT: F1 font bootstrap runs before QApplication construction.
  // Missing or unresolvable preference truth leaves platform defaults intact;
  // the later theme setFont remains the deliberate widgets baseline. See
  // docs/wiki/architecture/font-preferences.md.
  QindaQt::Services::FontDiscovery::FontSessionBootstrap::
      applyFromSessionSettings();
  QApplication application(argc, argv);
  application.setApplicationName(QStringLiteral("qindaqt-terminal"));
  application.setApplicationDisplayName(QStringLiteral("QindaQt Terminal"));
  application.setOrganizationName(QStringLiteral("QindaQt"));
  application.setDesktopFileName(QStringLiteral("org.qindaqt.Terminal"));
  // AGENT-GUARD: Without this, hiding the only window on close would end the
  // event loop before the bounded session escalation fires a single tick and
  // the teardown guarantee would be defeated (P1 review defect). The queued
  // closeShutdownFinished -> quit connection below is then the only quit
  // path, and it runs strictly after the child is confirmed gone.
  TerminalWindow::prepareApplicationQuitFlow(application);
  QCommandLineParser parser;
  configureCommandLine(parser);
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
  const auto appearance = TerminalAppearanceAdapter::fromTheme(theme.theme);
  if (!appearance.ok()) {
    std::fprintf(stderr, "qindaqt-terminal: %s\n",
                 qPrintable(appearance.diagnostic));
    return 3;
  }
  application.setPalette(appearance.appearance->windowPalette);
  application.setFont(appearance.appearance->interfaceFont);
  if (parser.isSet(QStringLiteral("check-theme"))) {
    std::printf("%s qst-%d\n", qPrintable(appearance.appearance->sourceThemeId),
                QindaQt::DesignTokens::DesignTokens::qstRevision);
    return 0;
  }

  const QStringList baseEnvironment =
      QProcessEnvironment::systemEnvironment().toStringList();
  const auto environment =
      TerminalLaunchPolicy::childEnvironment(baseEnvironment);
  if (!environment.outcome.ok) {
    std::fprintf(stderr, "qindaqt-terminal: %s\n",
                 qPrintable(environment.outcome.diagnostic));
    return 4;
  }
  const auto resolution = TerminalLaunchPolicy::resolveShell(
      parser.value(QStringLiteral("shell")),
      parser.values(QStringLiteral("arg")),
      parser.value(QStringLiteral("working-directory")), baseEnvironment);
  if (!resolution.outcome.ok) {
    std::fprintf(stderr, "qindaqt-terminal: %s\n",
                 qPrintable(resolution.outcome.diagnostic));
    return 4;
  }

  const QStringList themeDirectories =
      themeSearchDirectories(parser.value(QStringLiteral("theme-directory")));
  const QString launchThemeId = parser.value(QStringLiteral("theme"));

  // Settings1 persistence (profiles, default profile, restore flag). The
  // client is constructed after the --check-theme exit above so this row
  // never touches a bus. A start failure is fail-closed: the profile
  // controller serves built-in defaults until a baseline arrives.
  QindaQt::Services::SettingsClient::QtSettingsTransport settingsTransport(
      QDBusConnection::sessionBus());
  QindaQt::Services::SettingsClient::SettingsClient settingsClient(
      settingsTransport, TerminalKeys::scopedKeys());
  QString settingsError;
  const bool settingsStarted = settingsClient.start(&settingsError);
  if (!settingsStarted) {
    std::fprintf(stderr,
                 "qindaqt-terminal: settings unavailable (%s); "
                 "built-in profile defaults apply\n",
                 qPrintable(settingsError));
  }
  TerminalProfileSettings profileSettings(settingsClient);

  PosixProcessMonitor monitor;
  const auto factory = makeBackendFactory(themeDirectories, launchThemeId);

  TerminalSessionContext context;
  context.baseEnvironment = baseEnvironment;
  context.fallbackProgram = resolution.request.program;
  context.fallbackArguments = resolution.request.arguments;
  context.workingDirectory = resolution.request.workingDirectory;
  auto collection = std::make_unique<TerminalSessionCollection>(
      std::move(context), std::move(factory), &monitor, TeardownBounds{},
      nullptr);

  DetachedTerminalLinkSpawner linkSpawner;
  MessageBoxTerminalLinkConfirmation linkConfirmation;
  TerminalLinkOpener linkOpener(TerminalLinkOpener::resolveXdgOpen(),
                                &linkSpawner, &linkConfirmation);

  TerminalWindow window(std::move(collection), *appearance.appearance,
                        availableThemeIds(themeDirectories), &profileSettings,
                        &linkOpener);
  TerminalAppearanceBinding appearanceBinding(
      application, window, themeDirectories,
      parser.isSet(QStringLiteral("theme"))
          ? parser.value(QStringLiteral("theme"))
          : QString());

  window.resize(800, 500);
  bool firstSessionStarted = false;
  const auto startFirstSession = [&window, &profileSettings, &settingsClient,
                                  &settingsStarted, &firstSessionStarted] {
    if (firstSessionStarted) {
      return;
    }
    const auto state = settingsClient.state();
    const bool definitiveFallback =
        !settingsStarted ||
        state == QindaQt::Services::SettingsClient::ClientState::Degraded ||
        (state == QindaQt::Services::SettingsClient::ClientState::Unavailable &&
         !settingsClient.lastError().isEmpty());
    // A successfully started client begins Unavailable while activation is
    // still pending. Do not race that transient state with startup: a
    // persisted default profile must win when an owner is discoverable.
    if (!profileSettings.baselineReceived() && !definitiveFallback) {
      return;
    }
    firstSessionStarted = true;
    window.newSessionWithDefaultProfile();
    if (auto *active = window.session();
        active != nullptr &&
        active->lastExit().kind == TerminalExitStatus::Kind::StartFailed) {
      std::fprintf(stderr, "qindaqt-terminal: %s\n",
                   qPrintable(active->lastExit().diagnostic));
    }
  };
  const auto scheduleFirstSession = [&window, &startFirstSession] {
    QTimer::singleShot(0, &window, startFirstSession);
  };
  QObject::connect(&profileSettings, &TerminalProfileSettings::profilesChanged,
                   &window, scheduleFirstSession);
  QObject::connect(
      &settingsClient,
      &QindaQt::Services::SettingsClient::SettingsClient::stateChanged, &window,
      scheduleFirstSession);
  window.connectQuitAfterCloseShutdown(application);
  // AGENT-GUARD: realize the window layout before the first PTY can publish
  // its prompt. TerminalSession attaches the backend before child start, but
  // a still-hidden top-level leaves qtermwidget at its constructor-sized grid
  // and that first screen can be discarded by the later real resize.
  window.show();
  // AGENT-CONTRACT: first-party global-menu export — the shared composition
  // also used by the File Manager and Text Editor. Composed after show() so
  // the widget's platform QWindow exists; retained for the window lifetime
  // and destroyed before it.
  std::unique_ptr<QObject> menuExport = composeTerminalMenuExport(window);
  scheduleFirstSession();
  return application.exec();
}
