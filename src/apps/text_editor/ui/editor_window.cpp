// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_window.h"

#include "app_shell/native_file_selection_adapter.h"
#include "editor_application.h"
#include "ui/document_title.h"
#include "ui/editor_document_view.h"
#include "ui/find_replace_bar.h"

#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QPaintEvent>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace QindaQt::Apps::TextEditor {

EditorWindow::EditorWindow(
    DocumentStoreFactory storeFactory,
    std::unique_ptr<FileSelectionAdapter> fileSelectionAdapter,
    TextEditorRestorePolicy *restorePolicy, EditorApplication *application,
    std::unique_ptr<DocumentDialogs> dialogs, QWidget *parent)
    : QMainWindow(parent),
      m_appShellBridge(fileSelectionAdapter
                           ? std::move(fileSelectionAdapter)
                           : std::make_unique<NativeFileSelectionAdapter>(this),
                       this),
      m_restorePolicy(restorePolicy), m_application(application),
      m_dialogs(dialogs ? std::move(dialogs) : std::make_unique<NativeDocumentDialogs>(this)) {
  setObjectName(QStringLiteral("qindaqtEditorWindow"));
  setAccessibleName(tr("QindaQt Text Editor"));
  resize(920, 680);
  setMinimumSize(420, 320);
  setAcceptDrops(true);

  m_document = new DocumentController(storeFactory(), this);
  createCentralSurface();
  createDocumentView();
  createActions();
  connectDocument();
  createMenus();
  createToolbar();
  applyChrome();
  publishAppShellProjection();
  updateDocumentPresentation();
  connectRestorePolicy();
  if (editor()) {
    editor()->setFocus(Qt::OtherFocusReason);
  }
}

void EditorWindow::setMenuExport(std::unique_ptr<QObject> menuExport) {
  m_menuExport = std::move(menuExport);
}

DocumentController *EditorWindow::controller() const {
  return m_document;
}

QPlainTextEdit *EditorWindow::editor() const {
  return m_view ? m_view->editor() : nullptr;
}

QindaQt::AppShell::ApplicationCoordinator &EditorWindow::appShellCoordinator() {
  return m_appShellBridge.coordinator();
}

void EditorWindow::createCentralSurface() {
  auto *surface = new QWidget(this);
  surface->setObjectName(QStringLiteral("editorSurface"));
  auto *layout = new QVBoxLayout(surface);
  layout->setContentsMargins(12, 4, 12, 0);
  layout->setSpacing(0);

  m_findBar = new FindReplaceBar(surface);
  m_surfaceLayout = layout;
  layout->addWidget(m_findBar);
  setCentralWidget(surface);
  statusBar()->setObjectName(QStringLiteral("editorStatusBar"));
  statusBar()->setAccessibleName(tr("Editor status"));
  statusBar()->showMessage(tr("Ready"));
}

void EditorWindow::connectDocument() {
  connect(m_findBar, &FindReplaceBar::closed, this, [this] {
    if (editor()) {
      editor()->setFocus(Qt::ShortcutFocusReason);
    }
    updateActionStates();
  });
  connect(&m_appShellBridge.coordinator(),
          &QindaQt::AppShell::ApplicationCoordinator::quitDecisionRequested,
          this, [this](quint64 requestId, const QString &) {
            (void)m_appShellBridge.coordinator().resolveQuit(
                requestId, confirmWindowClose() == PendingAction::Continue);
          });
  connect(&m_appShellBridge.coordinator(),
          &QindaQt::AppShell::ApplicationCoordinator::quitApproved, this,
          [this](quint64) {
            persistRestoreState();
            m_pendingCloseApproved = true;
          });
}

void EditorWindow::createDocumentView() {
  m_view = new EditorDocumentView(m_document, centralWidget());
  m_surfaceLayout->insertWidget(0, m_view, 1);
  connect(m_view, &EditorDocumentView::presentationChanged, this,
          [this] { updateDocumentPresentation(); });
  connect(m_view, &EditorDocumentView::reloadRequested, this,
          &EditorWindow::reloadInteractively);
  connect(m_view, &EditorDocumentView::saveAsRequested, this,
          [this] { (void)saveDocumentAs(); });
  connect(m_document, &DocumentController::stateChanged, this,
          &EditorWindow::documentInventoryChanged);
  connect(m_view->editor(), &QPlainTextEdit::undoAvailable, this,
          [this] { if (m_actions.fileSave) updateActionStates(); });
  connect(m_view->editor(), &QPlainTextEdit::redoAvailable, this,
          [this] { if (m_actions.fileSave) updateActionStates(); });
  connect(m_view->editor(), &QPlainTextEdit::copyAvailable, this,
          [this] { if (m_actions.fileSave) updateActionStates(); });
}

void EditorWindow::updateDocumentPresentation(DocumentController *) {
  if (!m_view) return;
  const auto &state = m_document->state();
  QString name = sanitizeDocumentTitle(state.isUntitled()
      ? tr("Untitled") : QFileInfo(state.path()).fileName());
  if (name.isEmpty()) name = tr("Untitled");
  setWindowTitle(tr("%1[*] — QindaQt Text Editor").arg(name));
  setWindowFilePath(state.path());
  setWindowModified(state.isDirty());
  statusBar()->showMessage(m_view->statusText());
  if (m_actions.fileSave) updateActionStates();
}

EditorWindow::PendingAction
EditorWindow::confirmDocumentClose() {
  DocumentController *document = m_document;
  if (!document || !document->state().isDirty()) {
    return PendingAction::Continue;
  }
  const auto choice = m_dialogs->confirmClose();
  if (choice == DocumentCloseDecision::Cancel ||
      (choice == DocumentCloseDecision::Save && !saveDocument())) {
    return PendingAction::Cancel;
  }
  return PendingAction::Continue;
}

EditorWindow::PendingAction EditorWindow::confirmWindowClose() {
  return confirmDocumentClose();
}

bool EditorWindow::addPath(const QString &path, QString *diagnostic) {
  if (m_application) return m_application->openDocuments({path}, this, diagnostic);
  if (!m_document->state().isUntitled() || m_document->state().isDirty()) {
    if (m_document->state().path() == DocumentController::normalizePath(path))
      return true;
    if (diagnostic) *diagnostic = tr("A new document needs an application window owner");
    return false;
  }
  const auto result = m_document->openPath(path);
  if (!result.ok() && diagnostic) *diagnostic = result.diagnostic.left(512);
  return result.ok();
}

bool EditorWindow::openDocuments(const QStringList &paths, QString *diagnostic) {
  if (m_application) return m_application->openDocuments(paths, this, diagnostic);
  bool complete = true;
  for (const QString &path : paths) {
    QString current;
    if (!addPath(path, &current)) {
      if (complete && diagnostic) *diagnostic = current;
      complete = false;
    }
  }
  return complete;
}

void EditorWindow::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasUrls()) {
    const QList<QUrl> urls = event->mimeData()->urls();
    const bool localAndBounded =
        urls.size() <= DocumentCollection::maximumDocuments &&
        std::all_of(urls.cbegin(), urls.cend(),
                    [](const QUrl &url) { return url.isLocalFile(); });
    if (localAndBounded) {
      event->acceptProposedAction();
    }
  }
}

void EditorWindow::dropEvent(QDropEvent *event) {
  QStringList paths;
  for (const QUrl &url : event->mimeData()->urls()) {
    if (!url.isLocalFile() ||
        paths.size() >= DocumentCollection::maximumDocuments) {
      event->ignore();
      return;
    }
    paths.append(url.toLocalFile());
  }
  QString diagnostic;
  if (!openDocuments(paths, &diagnostic)) {
    announceStatus(diagnostic);
  }
  event->acceptProposedAction();
}

void EditorWindow::closeEvent(QCloseEvent *event) {
  // AGENT-GUARD: AppShell's exact quit ID is the only close authority. The
  // synchronous application decision may clear the coordinator request before
  // requestQuit returns, so approval is read only from quitApproved.
  m_pendingCloseApproved = false;
  (void)m_appShellBridge.coordinator().requestQuit(
      QStringLiteral("window-close"));
  if (m_pendingCloseApproved) {
    event->accept();
    emit closeAccepted();
  } else {
    event->ignore();
  }
}

void EditorWindow::paintEvent(QPaintEvent *event) {
  QMainWindow::paintEvent(event);
  if (!m_firstFramePublished) {
    m_firstFramePublished = true;
    emit firstFramePainted();
  }
}

void EditorWindow::changeEvent(QEvent *event) {
  QMainWindow::changeEvent(event);
  // Icon tints are the one palette-derived presentation the window keeps; the
  // style itself reacts to palette/theme changes without any help.
  if (m_findBar && (event->type() == QEvent::PaletteChange ||
                    event->type() == QEvent::StyleChange ||
                    event->type() == QEvent::ThemeChange)) {
    applyChrome();
  }
}

void EditorWindow::announceStatus(const QString &message) {
  const QString bounded = message.left(512);
  statusBar()->showMessage(bounded);
  QAccessibleAnnouncementEvent event(statusBar(), bounded);
  event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
  QAccessible::updateAccessibility(&event);
}

} // namespace QindaQt::Apps::TextEditor
