// SPDX-License-Identifier: GPL-3.0-or-later
#include "document/local_document_store.h"
#include "restore/restore_state_store.h"
#include "restore/text_editor_restore_policy.h"
#include "ui/editor_appearance.h"
#include "ui/editor_window.h"

#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/font_discovery/font_session_bootstrap.h"
#include "qindaqt/themes/theme_loader.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QWindow>

#include <cstdio>
#include <memory>

#include <qindaqt/app_shell/menu_export/first_party_composition.h>

namespace {

QStringList themeSearchDirectories(const QString &explicitDirectory) {
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

QindaQt::Themes::LoadResult loadTheme(const QString &themeId,
                                      const QStringList &directories) {
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

QString editorStateDirectory() {
  QString root = qEnvironmentVariable("XDG_STATE_HOME");
  if (root.isEmpty()) {
    root = QDir::home().filePath(QStringLiteral(".local/state"));
  }
  return QDir(root).filePath(QStringLiteral("qindaqt/text-editor"));
}

} // namespace

int main(int argc, char **argv) {
  using namespace QindaQt::Apps::TextEditor;
  using namespace QindaQt::Services::SettingsClient;

  QElapsedTimer startupTimer;
  startupTimer.start();
  // AGENT-CONTRACT: F1 font bootstrap — the single guarded composition-root
  // call runs before QApplication construction (pre-construction
  // QGuiApplication::setFont persists as the application default font). A
  // missing, unavailable, or unresolvable preference source leaves platform
  // defaults untouched (fail-closed). The theme baseline setFont below
  // remains the deliberate widgets baseline. See
  // docs/wiki/architecture/font-preferences.md.
  QindaQt::Services::FontDiscovery::FontSessionBootstrap::applyFromSessionSettings();
  QApplication application(argc, argv);
  application.setApplicationName(QStringLiteral("qindaqt-editor"));
  application.setApplicationDisplayName(QStringLiteral("QindaQt Text Editor"));
  application.setOrganizationName(QStringLiteral("QindaQt"));
  application.setDesktopFileName(QStringLiteral("org.qindaqt.TextEditor"));

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("QindaQt UTF-8 text editor"));
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addOption({QStringLiteral("theme"),
                    QStringLiteral("QindaQt theme identifier"),
                    QStringLiteral("id"), QStringLiteral("qinda-dark")});
  parser.addOption({QStringLiteral("theme-directory"),
                    QStringLiteral("Additional local theme directory"),
                    QStringLiteral("path")});
  parser.addOption(
      {QStringLiteral("check-theme"),
       QStringLiteral("Validate the selected theme through QST-1 and exit")});
  parser.addOption(
      {QStringLiteral("report-startup"),
       QStringLiteral("Print milliseconds to the first painted frame")});
  parser.addOption(
      {QStringLiteral("check-open-paths"),
       QStringLiteral("Load positional documents, report paths, and exit")});
  parser.addPositionalArgument(QStringLiteral("files"),
                               QStringLiteral("Local UTF-8 files to open"),
                               QStringLiteral("[file...]"));
  parser.process(application);
  const QStringList paths = parser.positionalArguments();
  if (paths.size() > DocumentCollection::maximumDocuments) {
    std::fprintf(stderr, "qindaqt-editor: at most %d documents may be opened\n",
                 DocumentCollection::maximumDocuments);
    return 2;
  }

  const auto theme = loadTheme(
      parser.value(QStringLiteral("theme")),
      themeSearchDirectories(parser.value(QStringLiteral("theme-directory"))));
  if (!theme.ok) {
    std::fprintf(stderr, "qindaqt-editor: %s\n", qPrintable(theme.error));
    return 3;
  }
  const auto appearance = EditorAppearanceAdapter::fromTheme(theme.theme);
  if (!appearance.ok()) {
    std::fprintf(stderr, "qindaqt-editor: %s\n",
                 qPrintable(appearance.diagnostic));
    return 3;
  }
  application.setPalette(appearance.appearance->palette);
  application.setFont(appearance.appearance->interfaceFont);
  if (parser.isSet(QStringLiteral("check-theme"))) {
    std::printf("%s qst-%d\n", qPrintable(appearance.appearance->sourceThemeId),
                QindaQt::DesignTokens::DesignTokens::qstRevision);
    return 0;
  }

  const DocumentStoreFactory factory = [] {
    return std::make_unique<LocalDocumentStore>();
  };
  if (parser.isSet(QStringLiteral("check-open-paths"))) {
    // The CLI admission proof needs document policy only. Exiting before
    // Settings1 composition guarantees the isolated row cannot discover or
    // activate an ambient session-bus service.
    EditorWindow window(factory, *appearance.appearance);
    QString diagnostic;
    if (!window.openDocuments(paths, &diagnostic)) {
      std::fprintf(stderr, "qindaqt-editor: %s\n", qPrintable(diagnostic));
      return 4;
    }
    std::printf("open-documents=%d\n", window.documents()->count());
    for (const QString &path : window.documents()->openPaths()) {
      std::printf("path=%s\n", qPrintable(path));
    }
    return 0;
  }

  // Settings1 owns only the Boolean policy. The editor-owned state file below
  // contains paths and active index only; check-theme exits before either
  // collaborator can touch a bus or user-state path.
  QtSettingsTransport settingsTransport(QDBusConnection::sessionBus());
  SettingsClient settingsClient(settingsTransport,
                                TextEditorKeys::scopedKeys());
  QString settingsError;
  if (!settingsClient.start(&settingsError)) {
    std::fprintf(stderr, "qindaqt-editor: settings unavailable (%s)\n",
                 qPrintable(settingsError));
  }
  TextEditorRestorePolicy restorePolicy(settingsClient);
  RestoreStateStore restoreStore(editorStateDirectory());
  EditorWindow window(factory, *appearance.appearance, nullptr, &restorePolicy,
                      &restoreStore);
  if (parser.isSet(QStringLiteral("report-startup"))) {
    QObject::connect(
        &window, &EditorWindow::firstFramePainted, &window,
        [&startupTimer] {
          std::printf("startup-first-frame-ms=%lld\n",
                      static_cast<long long>(startupTimer.elapsed()));
          std::fflush(stdout);
        },
        Qt::SingleShotConnection);
  }
  if (!paths.isEmpty()) {
    QString diagnostic;
    if (!window.openDocuments(paths, &diagnostic)) {
      std::fprintf(stderr, "qindaqt-editor: %s\n", qPrintable(diagnostic));
      return 4;
    }
  }
  window.restoreIfEnabled();
  window.show();
  // AGENT-CONTRACT: first-party global-menu export — the same shared
  // composition the File Manager and Terminal use (docs/wiki/shell/
  // global-menu.md). Composed after show() so the widget's platform QWindow
  // exists; retained for the window lifetime and destroyed before it. A
  // missing session bus or registrar leaves the export disabled/waiting and
  // the local QMenuBar stays the only authority.
  std::unique_ptr<QObject> menuExport;
  if (QWindow *windowHandle = window.windowHandle()) {
    menuExport = QindaQt::AppShell::MenuExport::composeFirstPartyMenuExport(
        window.appShellCoordinator(), *windowHandle,
        QDBusConnection::sessionBus());
  }
  return application.exec();
}
