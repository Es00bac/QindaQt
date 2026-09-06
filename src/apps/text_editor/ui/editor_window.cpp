// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_window.h"

#include "app_shell/native_file_selection_adapter.h"
#include "document/close_consent.h"
#include "restore/restore_state_store.h"
#include "ui/document_title.h"
#include "ui/editor_document_view.h"
#include "ui/find_replace_bar.h"

#include <QAbstractButton>
#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QMessageBox>
#include <QMimeData>
#include <QPaintEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace QindaQt::Apps::TextEditor {

EditorWindow::EditorWindow(
    DocumentStoreFactory storeFactory, EditorAppearance appearance,
    std::unique_ptr<FileSelectionAdapter> fileSelectionAdapter,
    TextEditorRestorePolicy *restorePolicy, RestoreStateStore *restoreStore,
    QWidget *parent)
    : QMainWindow(parent), m_appearance(std::move(appearance)),
      m_appShellBridge(fileSelectionAdapter
                           ? std::move(fileSelectionAdapter)
                           : std::make_unique<NativeFileSelectionAdapter>(this),
                       this),
      m_restorePolicy(restorePolicy), m_restoreStore(restoreStore) {
  setObjectName(QStringLiteral("qindaqtEditorWindow"));
  setAccessibleName(tr("QindaQt Text Editor"));
  resize(920, 680);
  setMinimumSize(420, 320);
  setAcceptDrops(true);
  setPalette(m_appearance.palette);
  setFont(m_appearance.interfaceFont);

  m_documents = new DocumentCollection(std::move(storeFactory), this);
  createCentralSurface();
  connectCollection();
  createActions();
  const AddDocumentResult initial = m_documents->addUntitled();
  Q_ASSERT(initial.ok());
  createMenus();
  publishAppShellProjection();
  updateDocumentPresentation();
  connectRestorePolicy();
  if (editor()) {
    editor()->setFocus(Qt::OtherFocusReason);
  }
}

void EditorWindow::applyAppearance(const EditorAppearance &appearance) {
  m_appearance = appearance;
  setPalette(appearance.palette);
  setFont(appearance.interfaceFont);
  for (EditorDocumentView *view : std::as_const(m_views)) {
    view->applyAppearance(appearance);
  }
}

DocumentController *EditorWindow::controller() const {
  return m_documents->at(m_tabs->currentIndex());
}

QPlainTextEdit *EditorWindow::editor() const {
  DocumentController *active = controller();
  EditorDocumentView *view = m_views.value(active);
  return view ? view->editor() : nullptr;
}

QindaQt::AppShell::ApplicationCoordinator &EditorWindow::appShellCoordinator() {
  return m_appShellBridge.coordinator();
}

void EditorWindow::createCentralSurface() {
  auto *surface = new QWidget(this);
  surface->setObjectName(QStringLiteral("editorSurface"));
  auto *layout = new QVBoxLayout(surface);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_tabs = new QTabWidget(surface);
  m_tabs->setObjectName(QStringLiteral("documentTabs"));
  m_tabs->setTabsClosable(true);
  m_tabs->setMovable(false);
  m_tabs->setDocumentMode(true);
  m_tabs->tabBar()->setAccessibleName(tr("Document tabs"));
  m_tabs->tabBar()->setAccessibleDescription(
      tr("Switch among open text documents"));
  m_findBar = new FindReplaceBar(surface);
  layout->addWidget(m_tabs, 1);
  layout->addWidget(m_findBar);
  setCentralWidget(surface);
  statusBar()->setObjectName(QStringLiteral("editorStatusBar"));
  statusBar()->setAccessibleName(tr("Editor status"));
  statusBar()->showMessage(tr("Ready"));
}

void EditorWindow::connectCollection() {
  connect(m_documents, &DocumentCollection::documentAdded, this,
          &EditorWindow::addDocumentView);
  connect(m_documents, &DocumentCollection::documentAboutToRemove, this,
          &EditorWindow::removeDocumentView);
  connect(m_documents, &DocumentCollection::documentsChanged, this,
          &EditorWindow::persistRestoreState);
  connect(m_tabs, &QTabWidget::currentChanged, this, [this] {
    updateDocumentPresentation();
    persistRestoreState();
    if (editor()) {
      editor()->setFocus(Qt::TabFocusReason);
    }
  });
  connect(m_tabs, &QTabWidget::tabCloseRequested, this,
          &EditorWindow::closeTab);
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

void EditorWindow::addDocumentView(DocumentController *controller,
                                   const int index) {
  auto *view = new EditorDocumentView(controller, m_appearance, m_tabs);
  m_views.insert(controller, view);
  m_tabs->insertTab(index, view, tr("Untitled"));
  connect(view, &EditorDocumentView::presentationChanged, this,
          [this, controller] { updateDocumentPresentation(controller); });
  connect(view, &EditorDocumentView::reloadRequested, this, [this, controller] {
    reloadInteractively(m_documents->indexOf(controller));
  });
  connect(view, &EditorDocumentView::saveAsRequested, this, [this, controller] {
    (void)saveDocumentAs(m_documents->indexOf(controller));
  });
  connect(view->editor(), &QPlainTextEdit::undoAvailable, this,
          [this] { updateActionStates(); });
  connect(view->editor(), &QPlainTextEdit::redoAvailable, this,
          [this] { updateActionStates(); });
  connect(view->editor(), &QPlainTextEdit::copyAvailable, this,
          [this] { updateActionStates(); });
  m_tabs->setCurrentIndex(index);
  updateDocumentPresentation(controller);
}

void EditorWindow::removeDocumentView(DocumentController *controller,
                                      const int index) {
  EditorDocumentView *view = m_views.take(controller);
  if (index >= 0 && index < m_tabs->count()) {
    m_tabs->removeTab(index);
  }
  delete view;
}

void EditorWindow::updateDocumentPresentation(DocumentController *changed) {
  if (changed) {
    const int index = m_documents->indexOf(changed);
    if (index >= 0) {
      const QString raw = changed->state().isUntitled()
                              ? tr("Untitled")
                              : QFileInfo(changed->state().path()).fileName();
      QString title = sanitizeDocumentTitle(raw);
      if (title.isEmpty()) {
        title = tr("Untitled");
      }
      if (changed->state().isDirty()) {
        title.append(QLatin1Char('*'));
      }
      m_tabs->setTabText(index, title);
      m_tabs->setTabToolTip(index, changed->state().path());
    }
  } else {
    for (int index = 0; index < m_documents->count(); ++index) {
      updateDocumentPresentation(m_documents->at(index));
    }
  }

  DocumentController *active = controller();
  EditorDocumentView *view = m_views.value(active);
  if (!active || !view) {
    return;
  }
  QString name = m_tabs->tabText(m_tabs->currentIndex());
  if (name.endsWith(QLatin1Char('*'))) {
    name.chop(1);
  }
  setWindowTitle(tr("%1[*] — QindaQt Text Editor").arg(name));
  setWindowFilePath(active->state().path());
  setWindowModified(active->state().isDirty());
  statusBar()->showMessage(view->statusText());
  updateActionStates();
}

EditorWindow::PendingAction
EditorWindow::confirmDocumentClose(const int index) {
  DocumentController *document = m_documents->at(index);
  if (!document || !document->state().isDirty()) {
    return PendingAction::Continue;
  }
  const QMessageBox::StandardButton choice = QMessageBox::warning(
      this, tr("Unsaved changes"), tr("Save this document before closing it?"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
      QMessageBox::Save);
  if (choice == QMessageBox::Cancel ||
      (choice == QMessageBox::Save && !saveDocument(index))) {
    return PendingAction::Cancel;
  }
  return PendingAction::Continue;
}

EditorWindow::PendingAction EditorWindow::confirmWindowClose() {
  QList<DocumentController *> docs;
  bool anyDirty = false;
  for (int index = 0; index < m_documents->count(); ++index) {
    docs.append(m_documents->at(index));
    anyDirty = anyDirty || m_documents->at(index)->state().isDirty();
  }
  if (!anyDirty) {
    return PendingAction::Continue;
  }

  QMessageBox prompt(QMessageBox::Warning, tr("Unsaved documents"),
                     tr("Save changes to these documents before closing?\n%1")
                         .arg(boundedDirtyDocumentSummary(docs)),
                     QMessageBox::NoButton, this);
  QAbstractButton *saveAll =
      prompt.addButton(tr("Save All"), QMessageBox::AcceptRole);
  QAbstractButton *discardAll =
      prompt.addButton(tr("Discard All"), QMessageBox::DestructiveRole);
  QAbstractButton *cancel = prompt.addButton(QMessageBox::Cancel);
  prompt.setDefaultButton(qobject_cast<QPushButton *>(saveAll));
  prompt.exec();
  CloseChoice choice = CloseChoice::Cancel;
  if (prompt.clickedButton() == saveAll) {
    choice = CloseChoice::SaveAll;
  } else if (prompt.clickedButton() == discardAll) {
    choice = CloseChoice::DiscardAll;
  } else if (prompt.clickedButton() != cancel) {
    return PendingAction::Cancel;
  }
  const ClosePlan plan = makeClosePlan(docs, choice);
  if (!plan.proceed) {
    return PendingAction::Cancel;
  }
  for (const int index : plan.saveIndexes) {
    if (!saveDocument(index)) {
      return PendingAction::Cancel;
    }
  }
  return PendingAction::Continue;
}

void EditorWindow::closeTab(const int index) {
  if (confirmDocumentClose(index) == PendingAction::Cancel) {
    return;
  }
  (void)m_documents->removeAt(index);
  if (m_documents->count() == 0) {
    (void)m_documents->addUntitled();
  }
}

bool EditorWindow::addPath(const QString &path, QString *diagnostic) {
  const int existing = m_documents->indexOfPath(path);
  if (existing >= 0) {
    selectTab(existing);
    return true;
  }
  DocumentController *initial =
      m_documents->count() == 1 ? m_documents->at(0) : nullptr;
  if (initial && initial->state().isUntitled() && !initial->state().isDirty()) {
    const DocumentOperation opened = initial->openPath(path);
    if (!opened.ok()) {
      if (diagnostic) {
        *diagnostic = opened.diagnostic.left(512);
      }
      return false;
    }
    updateDocumentPresentation(initial);
    return true;
  }
  const AddDocumentResult result = m_documents->openPath(path);
  if (!result.ok()) {
    if (diagnostic) {
      *diagnostic = result.operation.diagnostic.left(512);
    }
    return false;
  }
  selectTab(result.index);
  return true;
}

bool EditorWindow::openDocuments(const QStringList &paths,
                                 QString *diagnostic) {
  m_explicitStartupPaths = true;
  bool complete = true;
  for (const QString &path : paths) {
    QString currentDiagnostic;
    if (!addPath(path, &currentDiagnostic)) {
      if (complete && diagnostic) {
        *diagnostic = currentDiagnostic;
      }
      complete = false;
    }
  }
  persistRestoreState();
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
  m_pendingCloseApproved ? event->accept() : event->ignore();
}

void EditorWindow::paintEvent(QPaintEvent *event) {
  QMainWindow::paintEvent(event);
  if (!m_firstFramePublished) {
    m_firstFramePublished = true;
    emit firstFramePainted();
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
