// SPDX-License-Identifier: GPL-3.0-or-later
#include "links/terminal_link_opener.h"
#include "profiles/terminal_profile_settings.h"
#include "session/process_liveness.h"
#include "session/terminal_launch_policy.h"
#include "session/terminal_session.h"
#include "session/terminal_session_collection.h"
#include "ui/terminal_appearance.h"
#include "ui/terminal_startup.h"
#include "ui/terminal_widget_adapter.h"
#include "ui/terminal_window.h"

#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"

#include <QAccessibilityHints>
#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QFontDatabase>
#include <QMenuBar>
#include <QProcessEnvironment>
#include <QStyleHints>
#include <QTimer>
#include <QWindow>

#include <cstdio>
#include <memory>

#include <qindaqt/app_shell/menu_export/first_party_composition.h>

namespace QindaQt::Apps::Terminal {
namespace {

// AGENT-CONTRACT (ADR-0115/ADR-0116): palette, interface/monospace fonts,
// icon theme, and contrast hints come from the Qt platform theme; there is
// deliberately no QST token projection, per-app theme load, or font
// bootstrap here. Terminal CONTENT keeps its own derivation (ADR-0112): the
// ANSI protocol palette fitted to the content surface.
void configureCommandLine(QCommandLineParser &parser) {
  parser.setApplicationDescription(
      QStringLiteral("QindaQt terminal for the configured shell"));
  parser.addHelpOption();
  parser.addVersionOption();
  // AGENT-NOTE: --theme/--theme-directory are accepted and ignored. ADR-0116
  // retired per-app QST themes, but external harnesses (the global-menu
  // private-bus rows) still pass them.
  parser.addOption({QStringLiteral("theme"),
                    QStringLiteral("Deprecated no-op (ADR-0116): the Qt platform theme styles the app"),
                    QStringLiteral("id")});
  parser.addOption({QStringLiteral("theme-directory"),
                    QStringLiteral("Deprecated no-op (ADR-0116): no per-app theme catalog is read"),
                    QStringLiteral("path")});
  parser.addOption({QStringLiteral("shell"),
                    QStringLiteral("Absolute shell executable path"),
                    QStringLiteral("program")});
  parser.addOption({QStringLiteral("working-directory"),
                    QStringLiteral("Initial working directory"),
                    QStringLiteral("directory")});
  parser.addOption({QStringLiteral("profile"),
                    QStringLiteral("Saved profile identifier"),
                    QStringLiteral("id")});
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

// The factory resolves the profile's content scheme against the live platform
// palette per session. An unknown scheme id cannot reach here (profiles are
// validated), but a defensive System fallback keeps creation total.
// Captured by value: the factory outlives this call inside
// TerminalSessionCollection.
[[nodiscard]] TerminalSession::BackendFactory makeBackendFactory() {
  return [](const TerminalProfile &profile) {
    const auto scheme =
        terminalContentSchemeForId(profile.colorSchemeId)
            .value_or(TerminalContentScheme::System);
    const auto highContrast = terminalPlatformHighContrast();
    return std::unique_ptr<TerminalSessionBackend>(new TerminalWidgetAdapter(
        TerminalAppearanceAdapter::derive(
            QGuiApplication::palette(), scheme, highContrast,
            QFontDatabase::systemFont(QFontDatabase::FixedFont)),
        profile));
  };
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

  // Settings1 persistence (profiles, default profile, restore preference).
  // A start failure is fail-closed: the profile controller serves built-in
  // defaults until a baseline arrives.
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
  const auto factory = makeBackendFactory();

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

  const auto launchAnotherTerminal = makeNewTerminalLauncher(parser);

  TerminalWindow window(std::move(collection),
                        terminalDesktopContentAppearance(),
                        &profileSettings, &linkOpener, launchAnotherTerminal);
  // Live desktop changes re-derive the desktop-scheme content appearance;
  // palette/font/style changes reach the window through its changeEvent, and
  // profile-pinned schemes and local zoom survive inside the adapter.
  if (auto *styleHints = QGuiApplication::styleHints()) {
    QObject::connect(styleHints, &QStyleHints::colorSchemeChanged, &window,
                     [&window] { window.refreshDesktopContentAppearance(); });
  }
  if (auto *hints = QGuiApplication::styleHints()->accessibility()) {
    QObject::connect(hints, &QAccessibilityHints::contrastPreferenceChanged,
                     &window,
                     [&window] { window.refreshDesktopContentAppearance(); });
  }

  window.resize(800, 500);
  bool firstSessionStarted = false;
  const QString requestedProfileId = parser.value(QStringLiteral("profile"));
  const auto startFirstSession = [&window, &profileSettings, &settingsClient,
                                  &settingsStarted, &firstSessionStarted,
                                  &requestedProfileId] {
    if (firstSessionStarted) {
      return;
    }
    const auto state = settingsClient.state();
    const bool definitiveFallback =
        !settingsStarted ||
        state == QindaQt::Services::SettingsClient::ClientState::Degraded ||
        (state == QindaQt::Services::SettingsClient::ClientState::Unavailable &&
         !settingsClient.lastError().isEmpty());
    if (!initialSessionReady(profileSettings, definitiveFallback)) {
      return;
    }
    firstSessionStarted = true;
    QString unavailableProfile;
    const TerminalProfile profile = initialSessionProfile(
        profileSettings, requestedProfileId, &unavailableProfile);
    if (!unavailableProfile.isEmpty()) {
      std::fprintf(stderr, "qindaqt-terminal: saved profile is unavailable: %s\n",
                   qPrintable(unavailableProfile));
    }
    window.startSession(profile);
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
