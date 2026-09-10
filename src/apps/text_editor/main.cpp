// SPDX-License-Identifier: GPL-3.0-or-later
#include "document/local_document_store.h"
#include "restore/recovery_journal_store.h"
#include "restore/restore_state_store.h"
#include "restore/text_editor_restore_policy.h"
#include "ui/editor_window.h"
#include "ui/editor_application.h"

#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDir>
#include <QElapsedTimer>
#include <QMenuBar>
#include <QWindow>

#include <cstdio>
#include <memory>

#include <qindaqt/app_shell/menu_export/first_party_composition.h>

namespace {

// AppShell's fixed menu object path is per bus connection. Each ordinary
// document window therefore owns a distinct connection, retired only after
// its exporter has withdrawn its endpoint and identity.
class WindowMenuExport final : public QObject {
public:
  WindowMenuExport(QindaQt::Apps::TextEditor::EditorWindow &window, quint64 sequence)
      : m_connectionName(QStringLiteral("qindaqt-editor-menu-%1-%2")
            .arg(QCoreApplication::applicationPid()).arg(sequence)) {
    auto bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, m_connectionName);
    if (QWindow *handle = window.windowHandle()) {
      m_export = QindaQt::AppShell::MenuExport::composeFirstPartyMenuExport(
          window.appShellCoordinator(), *handle, bus,
          [&window](bool visible) { window.menuBar()->setVisible(visible); });
    }
  }
  ~WindowMenuExport() override {
    m_export.reset();
    QDBusConnection::disconnectFromBus(m_connectionName);
  }
private:
  QString m_connectionName;
  std::unique_ptr<QObject> m_export;
};

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
  // AGENT-CONTRACT: Palette, interface and monospace fonts, icon theme, and
  // contrast hints come from the Qt platform theme (ADR-0115); there is
  // deliberately no QST token projection, per-app theme load, or font
  // bootstrap here (ADR-0116).
  QApplication application(argc, argv);
  application.setApplicationName(QStringLiteral("qindaqt-editor"));
  application.setApplicationDisplayName(QStringLiteral("QindaQt Text Editor"));
  application.setOrganizationName(QStringLiteral("QindaQt"));
  application.setDesktopFileName(QStringLiteral("org.qindaqt.TextEditor"));

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("QindaQt UTF-8 text editor"));
  parser.addHelpOption();
  parser.addVersionOption();
  // AGENT-NOTE: --theme/--theme-directory are accepted and ignored. ADR-0116
  // retired per-app QST themes, but external harnesses (the global-menu
  // private-bus rows, the installed runtime probe) still pass them.
  parser.addOption({QStringLiteral("theme"),
                    QStringLiteral("Deprecated no-op (ADR-0116): the Qt platform theme styles the app"),
                    QStringLiteral("id")});
  parser.addOption({QStringLiteral("theme-directory"),
                    QStringLiteral("Deprecated no-op (ADR-0116): no per-app theme catalog is read"),
                    QStringLiteral("path")});
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

  const DocumentStoreFactory factory = [] {
    return std::make_unique<LocalDocumentStore>();
  };
  if (parser.isSet(QStringLiteral("check-open-paths"))) {
    // The CLI admission proof needs document policy only. Exiting before
    // Settings1 composition guarantees the isolated row cannot discover or
    // activate an ambient session-bus service.
    EditorApplication editor(factory, nullptr, nullptr, nullptr, {}, false);
    QString diagnostic;
    if (!editor.start(paths, &diagnostic)) {
      std::fprintf(stderr, "qindaqt-editor: %s\n", qPrintable(diagnostic));
      return 4;
    }
    std::printf("open-documents=%d\n", int(editor.windows().size()));
    for (const QString &path : editor.openPaths()) {
      std::printf("path=%s\n", qPrintable(path));
    }
    return 0;
  }

  // Settings1 owns only the Boolean policy. The editor-owned state file below
  // contains paths and active index only.
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
  // Crash-recovery journals are app-local recovery state beside the
  // paths-only inventory, confined by the same openat/O_NOFOLLOW walk.
  RecoveryJournalStore journalStore(
      QDir(editorStateDirectory()).filePath(QStringLiteral("recovery")));
  EditorApplication editor(factory, &restorePolicy, &restoreStore,
                           &journalStore);
  bool startupReported = false;
  QObject::connect(&editor, &EditorApplication::windowCreated, &editor,
      [&](EditorWindow *window) {
        if (parser.isSet(QStringLiteral("report-startup"))) {
          QObject::connect(window, &EditorWindow::firstFramePainted, &editor,
              [&] {
                if (startupReported) return;
                startupReported = true;
                std::printf("startup-first-frame-ms=%lld\n",
                    static_cast<long long>(startupTimer.elapsed()));
                std::fflush(stdout);
              });
        }
      });
  quint64 menuWindowSequence = 0;
  QObject::connect(&editor, &EditorApplication::windowShown, &editor,
      [&menuWindowSequence](EditorWindow *window) {
        window->setMenuExport(std::make_unique<WindowMenuExport>(*window, ++menuWindowSequence));
      });
  QString diagnostic;
  if (!editor.start(paths, &diagnostic)) {
    std::fprintf(stderr, "qindaqt-editor: %s\n", qPrintable(diagnostic));
    if (editor.openPaths().isEmpty()) return 4;
  }
  return application.exec();
}
